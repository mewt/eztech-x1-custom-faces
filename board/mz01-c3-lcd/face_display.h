#ifndef MZ01_FACE_DISPLAY_H
#define MZ01_FACE_DISPLAY_H

#include "display/lcd_display.h"

#include <string>

#include "faces_data.h"

// Replaces the emoji glyph the stock UI shows with an animated face.
//
// The frames are pre-rendered RGB565 in flash — the panel's own pixel format —
// so LVGL draws them directly with no image decoder and no RAM copy. That
// matters on this board: the earlier GIF-decoder version held a ~46 KB
// ARGB8888 canvas on an ESP32-C3 with no PSRAM, which starved mbedtls and the
// Opus codec task until the firmware panicked. Animation here is just a timer
// swapping frame pointers, which costs nothing but flash.
//
// Emotions with no face in the pack fall back to `neutral`.
class FaceDisplay : public SpiLcdDisplay {
public:
    FaceDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                int width, int height, int offset_x, int offset_y,
                bool mirror_x, bool mirror_y, bool swap_xy,
                DisplayFonts fonts,
                lv_display_rotation_t sw_rotation = LV_DISPLAY_ROTATION_0);

    void SetEmotion(const char* emotion) override;
    void SetTheme(const std::string& theme_name) override;

private:
    static void TickCb(lv_timer_t* timer);
    void ShowFrame(uint8_t index);
    void LayOutFaceAndText();
    void ApplyTrueBlack();

    // Breathing room between the face and the line of text under it.
    static constexpr int FACE_TEXT_GAP_PX = 6;

    lv_obj_t* face_parent_ = nullptr;
    lv_obj_t* img_ = nullptr;
    lv_timer_t* timer_ = nullptr;
    lv_image_dsc_t frames_[FACE_MAX_FRAMES] = {};
    uint8_t frame_count_ = 0;
    uint8_t frame_index_ = 0;
    std::string current_emotion_;
};

#endif  // MZ01_FACE_DISPLAY_H
