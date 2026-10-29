# Animated faces for the EZTECH X1 / Zuowei MZ01-C3-LCD

Custom firmware for the little ESP32-C3 XiaoZhi (小智) AI speaker with the round
1.54" screen, replacing the static emoji glyphs (😶 🙂 😆) with **animated GIF
faces**, in **English**, on a **true-black** screen — and still talking to the
official `xiaozhi.me` cloud, so your agent and its MCP tools keep working.

```
         stock                          this
   ┌─────────────────┐           ┌─────────────────┐
   │ 待命      🔋    │           │ Standby    🔋   │
   │                 │           │                 │
   │       😶        │           │     ( ◕ ◡ ◕ )   │  <- animated, 96x96
   │                 │           │                 │
   │   你好，有什么    │           │                 │
   │   可以帮你的？    │           │  How can I help?│  <- pinned to the bottom
   └─────────────────┘           └─────────────────┘
      #1E1E1E grey                   #000000 black
```

## The device

Sold as the **EZTECH AI Robot X1**, branded **HaloBaby** on the case and on the
stock screen — which is why the offline wake word is "halo baby" (the VB6824
reports it as `hello baby`). The board inside is a **Zuowei MZ01-C3-LCD**,
factory board id `zuowei-c3-realtime-lcd-id`. Out of the box it runs a light
theme with static emoji glyphs; everything below replaces that.

> 🛒 **Where I bought mine:** [EZTECH AI Robot X1 on Shopee Indonesia](https://s.shopee.co.id/an_redir?origin_link=https%3A%2F%2Fshopee.co.id%2FEZTECH-AI-Robot-X1-Mendukung-DeepSeek-Percakapan-AI-Lancar-Belajar-Multi-Bahasa-%28Indonesia-Inggris-Mandarin%29-i.21079976.29390582188&affiliate_id=11313710234&sub_id=github)
>
> *Affiliate disclosure: that is a Shopee affiliate link. If you buy through it
> I earn a small commission at no extra cost to you. The plain link, with no
> tracking, is [here](https://shopee.co.id/EZTECH-AI-Robot-X1-Mendukung-DeepSeek-Percakapan-AI-Lancar-Belajar-Multi-Bahasa-(Indonesia-Inggris-Mandarin)-i.21079976.29390582188)
> — use that one if you'd rather not be tracked. I paid for mine; nobody is
> sponsoring this and the firmware works the same either way.*

| | |
|---|---|
| SoC | ESP32-C3 (RISC-V, single core), 8 MB flash, **no PSRAM** |
| Display | ST7789 1.54" **240×240** SPI — a 240×240 window inside 240×320 controller RAM |
| Audio | **VB6824** voice coprocessor on UART1 @ 2 Mbps: mic (opus 16 kHz in), speaker (PCM 16 kHz out), offline wake word "hello baby" |
| Power | single cell + latch on GPIO13, battery ADC on GPIO4, charge detect GPIO21 |
| Factory firmware | xiaozhi **1.7.6**, built 2025-07-18, IDF 5.4.2, in `ota_0` |
| Cloud | official `api.tenclass.net` (MQTT + websocket) |

Pin map (verified on real hardware in
[gizwits/OpenEmbodied#5](https://github.com/gizwits/OpenEmbodied/issues/5),
cross-checked against this device's own factory image):

```
LCD   MOSI=1  SCLK=3  CS=12  DC=0  RST=2  BL=5 (PWM)   SPI2, mode 3, 40 MHz, inverted
VB6824  TX=20 (C3->VB)  RX=10 (VB->C3)  UART1 @ 2 Mbps
Buttons BOOT=9  VOL+=8  VOL-=7
Power   battery ADC GPIO4 (557–680 mV)  charge detect GPIO21  power latch GPIO13
```

**GPIO13 is a power latch** — drive it high early in boot or the device switches
itself off the moment the button is released.

## Quick start

You need macOS or Linux, ESP-IDF **v5.4.2** in `~/esp/esp-idf`, ImageMagick
(`magick`) for the face renderer, and `esptool.py`.

```bash
git clone https://github.com/mewt/eztech-x1-custom-faces
cd eztech-x1-custom-faces
./install.sh            # clones DOIT_AI into ./firmware, patches it, renders the faces

tools/backup.sh         # DO THIS FIRST. 8 MB dump = your only way back
tools/build.sh
tools/flash.sh          # app only -> ota_0 @0x100000
tools/monitor.sh
```

`install.sh` is re-runnable. Flashing is **app-only**: the built partition table
is byte-identical to the factory one, so the bootloader, partition table and
NVS are untouched and your Wi-Fi credentials and device pairing survive.

Rollback:

```bash
BACKUP=your-backup.bin tools/restore.sh app    # factory app, keeps current NVS
BACKUP=your-backup.bin tools/restore.sh full   # all 8 MB (rewinds NVS too)
```

## Why this base

`install.sh` builds on [SmartArduino/DOIT_AI](https://github.com/SmartArduino/DOIT_AI)
(a xiaozhi-esp32 fork, v1.8.2). It is the only base with all three of:

* the **official XiaoZhi protocol** (`api.tenclass.net`, MQTT `hello` +
  websocket BinaryProtocol2/3), so the device keeps talking to your agent and
  its MCP tools,
* the **VB6824 UART codec** already integrated (`components/vb6824` ships the
  prebuilt `libesp32c3.a` plus `main/audio/codecs/vb6824_audio_codec.cc`),
* a v1-era **partition table identical to the factory one**, so only the app
  gets rewritten.

Two bases were rejected:

* [gizwits/OpenEmbodied](https://github.com/gizwits/OpenEmbodied) `pr-board` has
  the verified MZ01 board bring-up, but its protocol is **Coze** (`ws.coze.cn`,
  `chat.update`, `input_audio_buffer.*`) — flashing it disconnects the device
  from xiaozhi.me. Its pin map, `power_manager.h` and display-rotation fix are
  reused here with thanks.
* Upstream [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) v2.5.3 has a
  proper swappable emoji-pack system (an `assets` partition + `index.json`), but
  no VB6824 driver, and v1.x→v2.x needs a different partition table, so it
  cannot be reached by OTA
  ([#1296](https://github.com/78/xiaozhi-esp32/issues/1296)). Worth revisiting.

## Why the faces are not GIFs on the device

The first version embedded the GIFs and used LVGL's GIF decoder. It worked and
the faces appeared — and then the device boot-looped every 7 seconds:

```
E (3621) esp-tls: Failed to initialize TLS
E (4102) Application: Opus codec task is out of memory, free_sram: 8352
Guru Meditation Error: Core 0 panic'ed (Store access fault)
```

LVGL's decoder holds a `w*h*4` ARGB8888 canvas **plus** a `w*h` frame buffer for
as long as a GIF is open — about 46 KB at 96 px. On a PSRAM-less C3 that left
8 KB of internal RAM, so mbedtls could not initialise (no cloud), the Opus task
could not start, and the firmware then wrote through the failed allocation.

Note what this *isn't*: it is not the number of faces. Only one GIF is ever
open, so the face count costs flash, never RAM. The cost was the **size** of
the one decoded face.

So `tools/build_faces.py` pre-renders every frame to **little-endian RGB565** —
the panel's own pixel format — straight into flash. LVGL blits it with **no
decoder and no RAM copy**, and the animation is a `lv_timer` swapping
`lv_image_dsc_t` pointers. Decoder RAM: **zero**. Free SRAM went 8 KB → 96 KB,
which is what buys back the cloud connection and the audio path.

## The face pack

`tools/face_map.json` maps each of the 21 emotion names the server can send to
a GIF in `faces/`. Edit it freely — several emotions may share one file, and
anything unmapped falls back to `neutral`.

```bash
python3 tools/build_faces.py                      # 96px, 3 frames -> 1026 KB
python3 tools/build_faces.py --size 80 --frames 4 # 80px, 4 frames ->  950 KB
python3 tools/build_faces.py --size 120 --frames 2
```

Cost is `faces × frames × size × size × 2` bytes of flash and nothing in RAM,
so the trade is **face size vs. animation smoothness vs. flash**. At 96 px /
3 frames: 19 distinct faces covering 21 emotions, 1026 KB, ~290 KB still free in
the app partition. Per-face frame periods come from each GIF's own loop length,
clamped to 60–500 ms.

Want your own faces? Drop GIFs in a folder, point `_source_pack` at it, rerun.

**The GIFs in `faces/` are not mine** — they are a community pack from
[Loa AI](https://loaai.me/bai-viet/thay-icon-bieu-cam-cho-loa-ai-xiaozhi-emoji-gif-cho-robot-ai-chatbot),
with no stated author or licence. See [`faces/CREDITS.md`](faces/CREDITS.md).

## Layout, true black, English

**Layout.** The stock content area is a vertical flex box (emoji label, preview
image, chat text) centred as one group. `FaceDisplay` takes the face and the
text out of that layout and places them absolutely: face centred, text on the
bottom edge. The text gets only the whole lines that fit below the face and is
ellipsised past that, so a long reply can never creep up over the face.

**True black.** The stock dark theme is Material grey — `#121212` behind the
status bar, `#1E1E1E` behind the face, and `SetupUI` leaves LVGL's default card
border on the content area in `#333333`. The face frames are pure `#000000`
(81% of each frame), so the face sat in a visibly lighter box.
`ApplyTrueBlack()` blacks all of it out and drops the border, so the frames
blend into the panel. It applies only to the dark theme and re-applies after
`SetTheme()`, since the server can switch themes at runtime via
`self.screen.set_theme`. Black still is not *black*: the ST7789 is backlit, so
`#000000` is light through a closed shutter at roughly 800–1000:1. Lower the
backlight for a deeper black; only OLED actually fixes it.

**English.** `CONFIG_LANGUAGE_EN_US=y` selects `main/assets/en-US`, which
carries both the on-screen strings and the device's **spoken** prompts — the
`.p3` clips for activation, Wi-Fi setup and the activation-code digits are
per-language. Costs 8 KB more flash than Chinese. Two strings the locale files
don't cover are patched too: the vendor OTA screen (hard-coded Chinese) and the
wake phrase the board reported to the server (`你好小智` → what the VB6824
actually listens for). The wake word itself lives in the VB6824's own firmware;
your agent's reply language comes from the xiaozhi.me console, not the device.

## What's in here

```
board/mz01-c3-lcd/   the board: config.h, mz01-c3-lcd.cc, power_manager.h,
                     face_display.{h,cc}
patches/             the delta against DOIT_AI v1.8.2 — board entry in
                     CMakeLists/Kconfig, optional LVGL software rotation for
                     SpiLcdDisplay, LVGL port task priority 1->4, English OTA
                     screen, DEVICE_CODE string in all four locales
tools/               face pipeline + backup/build/flash/monitor/restore
faces/               the GIF pack (see CREDITS.md)
sdkconfig.mz01       board selection, VB6824 opus paths, CONFIG_LANGUAGE_EN_US
```

Two upstream gotchas worth knowing if you port this elsewhere:

* **LVGL software rotation is mandatory here.** This ST7789 exposes a 240×240
  window inside 240×320 controller RAM, so MADCTL hardware rotation mirrors and
  wraps the framebuffer. The panel stays in its native orientation and LVGL
  rotates (`sw_rotate`).
* **LVGL port task priority 1 deadlocks** `lvgl_port_add_disp` on this
  single-core part. The patch raises it to 4.

## Notes

* 1.x → 2.x cannot be done over OTA (different partition table).
* On factory firmware, **five quick presses of the middle button** triggers the
  vendor OTA at `vbota.esp32.cn`; another owner of this board reached 1.9.3
  that way.
* Vendor contact for newer factory firmware: `ai@zuoweitech.cn`.
* Your flash backup contains your **Wi-Fi passwords and the device's MQTT
  credential in cleartext**. Keep it out of shared folders and out of git —
  `.gitignore` already excludes `*.bin`.

## Thanks

[78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) for the project,
[SmartArduino/DOIT_AI](https://github.com/SmartArduino/DOIT_AI) for the VB6824
integration, [gizwits/OpenEmbodied](https://github.com/gizwits/OpenEmbodied)
and the reporter of issue #5 for the verified MZ01 pin map and the rotation fix,
and [Loa AI](https://loaai.me) for collecting the face pack.

MIT for the code — see [LICENSE](LICENSE), which carves out `faces/`.
