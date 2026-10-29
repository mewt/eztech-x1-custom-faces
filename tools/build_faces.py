#!/usr/bin/env python3
"""Turn a pack of animated GIF faces into pre-rendered RGB565 frames in flash.

Why not just embed the GIFs: LVGL's GIF decoder keeps an ARGB8888 canvas plus a
frame buffer alive for as long as a GIF is open (~46 KB at 96px). On this
ESP32-C3 there is no PSRAM, and that was enough to starve TLS and the Opus
codec task until the firmware panicked. Pre-rendered RGB565 matching the panel
format is drawn straight out of flash, so the decoder cost is zero and the
animation is just a timer swapping frame pointers.

The trade is flash: bytes = faces * frames * size * size * 2.

Usage:
    python3 tools/build_faces.py                      # 96px, 3 frames
    python3 tools/build_faces.py --size 80 --frames 4
"""

import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
DEFAULT_OUT = HERE.parent / "firmware" / "main" / "boards" / "mz01-c3-lcd" / "faces"

# Flash left for faces inside the 0x380000 app partition, measured against the
# 2.21 MB the firmware occupies without them. Keep a margin for code growth.
FLASH_BUDGET = 1_200_000


def sh(*cmd: str) -> str:
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        sys.exit(f"command failed: {' '.join(cmd)}\n{proc.stderr.strip()}")
    return proc.stdout


def c_identifier(name: str) -> str:
    return "face_" + re.sub(r"[^a-z0-9]+", "_", name.lower())


def gif_duration_ms(path: Path) -> int:
    """Total loop length, from the per-frame delays in centiseconds."""
    delays = [int(d) for d in sh("magick", "identify", "-format", "%T ", str(path)).split()]
    total = sum(d for d in delays) * 10
    return total if total > 0 else len(delays) * 60


def rgb_to_rgb565(raw: bytes) -> bytes:
    out = bytearray(len(raw) // 3 * 2)
    for i in range(0, len(raw), 3):
        r, g, b = raw[i], raw[i + 1], raw[i + 2]
        v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        out[i // 3 * 2] = v & 0xFF            # little endian, as LVGL expects
        out[i // 3 * 2 + 1] = (v >> 8) & 0xFF
    return bytes(out)


def render_frames(src: Path, size: int, want: int, tmp: Path) -> list[bytes]:
    """Coalesce the GIF, resize, and return `want` evenly spaced RGB565 frames."""
    stem = tmp / src.stem
    sh("magick", str(src), "-coalesce", "-resize", f"{size}x{size}!", "+repage",
       f"{stem}_%03d.rgb")
    files = sorted(stem.parent.glob(f"{src.stem}_*.rgb"))
    if not files:
        sys.exit(f"no frames rendered from {src}")

    # Evenly spaced picks across the loop, first frame always included.
    picks = [round(i * (len(files) - 1) / max(want - 1, 1)) for i in range(want)]
    picks = sorted(dict.fromkeys(min(p, len(files) - 1) for p in picks))

    frames = []
    expect = size * size * 3
    for p in picks:
        raw = files[p].read_bytes()
        if len(raw) != expect:
            sys.exit(f"{files[p].name}: expected {expect} raw bytes, got {len(raw)}")
        frames.append(rgb_to_rgb565(raw))
    return frames


def emit_array(fh, symbol: str, blob: bytes) -> None:
    fh.write(f"static const uint8_t {symbol}[] = {{\n")
    for i in range(0, len(blob), 16):
        fh.write("    " + ", ".join(f"0x{b:02x}" for b in blob[i : i + 16]) + ",\n")
    fh.write("};\n\n")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--size", type=int, default=96, help="square pixel size")
    ap.add_argument("--frames", type=int, default=3, help="frames kept per face")
    ap.add_argument("--map", type=Path, default=HERE / "face_map.json")
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT)
    args = ap.parse_args()

    if not shutil.which("magick"):
        sys.exit("ImageMagick 'magick' not found")
    if args.frames < 1:
        sys.exit("--frames must be >= 1")

    spec = json.loads(args.map.read_text())
    pack = Path(spec["_source_pack"])
    if not pack.is_absolute():
        pack = (HERE.parent / pack).resolve()  # relative to the repo root
    emotions = {k: v for k, v in spec.items() if not k.startswith("_")}

    frame_bytes = args.size * args.size * 2
    distinct = sorted(set(emotions.values()))
    total = len(distinct) * args.frames * frame_bytes
    if total > FLASH_BUDGET:
        sys.exit(f"{total/1e6:.2f} MB of frames exceeds the {FLASH_BUDGET/1e6:.2f} MB "
                 f"budget - reduce --size or --frames")

    args.out.mkdir(parents=True, exist_ok=True)
    renders: dict[str, list[bytes]] = {}
    periods: dict[str, int] = {}
    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        for name in distinct:
            src = pack / name
            if not src.is_file():
                sys.exit(f"source GIF missing: {src}")
            renders[name] = render_frames(src, args.size, args.frames, tmp)
            # Keep the original loop length, spread over the frames we kept.
            per = max(60, min(500, gif_duration_ms(src) // len(renders[name])))
            periods[name] = per

    symbols = {name: c_identifier(Path(name).stem) for name in distinct}

    header = args.out / "faces_data.h"
    header.write_text(
        "// Generated by tools/build_faces.py - do not edit.\n"
        "#ifndef MZ01_FACES_DATA_H\n"
        "#define MZ01_FACES_DATA_H\n\n"
        "#include <stdint.h>\n\n"
        "#ifdef __cplusplus\n"
        'extern "C" {\n'
        "#endif\n\n"
        "// Frames are raw little-endian RGB565, the panel's own format, so LVGL\n"
        "// draws them directly out of flash with no decoder and no RAM copy.\n"
        "typedef struct {\n"
        "    const char* emotion;\n"
        "    const uint8_t* const* frames;\n"
        "    uint8_t frame_count;\n"
        "    uint16_t period_ms;\n"
        "} face_anim_t;\n\n"
        f"#define FACE_SIZE_PX {args.size}\n"
        f"#define FACE_FRAME_BYTES {frame_bytes}\n"
        f"#define FACE_MAX_FRAMES {args.frames}\n"
        f"#define FACE_ANIM_COUNT {len(emotions)}\n\n"
        "extern const face_anim_t FACE_ANIMS[FACE_ANIM_COUNT];\n\n"
        "#ifdef __cplusplus\n"
        "}\n"
        "#endif\n\n"
        "#endif  // MZ01_FACES_DATA_H\n"
    )

    source = args.out / "faces_data.c"
    with source.open("w") as fh:
        fh.write("// Generated by tools/build_faces.py - do not edit.\n")
        fh.write(f"// pack={pack}  {args.size}x{args.size}px  {args.frames} frames/face\n\n")
        fh.write('#include "faces_data.h"\n\n')
        for name in distinct:
            sym = symbols[name]
            for i, blob in enumerate(renders[name]):
                emit_array(fh, f"{sym}_f{i}", blob)
            fh.write(f"static const uint8_t* const {sym}_frames[] = {{\n")
            for i in range(len(renders[name])):
                fh.write(f"    {sym}_f{i},\n")
            fh.write("};\n\n")
        fh.write("const face_anim_t FACE_ANIMS[FACE_ANIM_COUNT] = {\n")
        for emotion, name in emotions.items():
            sym = symbols[name]
            fh.write(f'    {{ "{emotion}", {sym}_frames, {len(renders[name])}, '
                     f"{periods[name]} }},\n")
        fh.write("};\n")

    actual = sum(len(b) for frames in renders.values() for b in frames)
    print(f"pack        : {pack}")
    print(f"frames      : {args.size}x{args.size}px RGB565, {args.frames}/face")
    print(f"distinct    : {len(distinct)} faces for {len(emotions)} emotions")
    print(f"flash total : {actual/1024:.0f} KB  (budget {FLASH_BUDGET/1024:.0f} KB)")
    print(f"decoder RAM : 0 bytes - drawn straight from flash")
    print()
    for name in distinct:
        users = [e for e, s in emotions.items() if s == name]
        print(f"  {name:18s} {len(renders[name])} frames @ {periods[name]:3d} ms"
              f"  -> {', '.join(users)}")
    print(f"\nwrote {source}\n      {header}")


if __name__ == "__main__":
    main()
