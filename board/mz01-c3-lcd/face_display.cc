#include "face_display.h"

#include <esp_log.h>

#include <cstring>

#define TAG "FaceDisplay"

namespace {

const face_anim_t* FindFace(const char* emotion) {
    if (emotion == nullptr) {
        return nullptr;
    }
    for (int i = 0; i < FACE_ANIM_COUNT; i++) {
        if (std::strcmp(FACE_ANIMS[i].emotion, emotion) == 0) {
            return &FACE_ANIMS[i];
        }
    }
    return nullptr;
}

}  // namespace

FaceDisplay::FaceDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                         int width, int height, int offset_x, int offset_y,
                         bool mirror_x, bool mirror_y, bool swap_xy,
                         DisplayFonts fonts,
                         lv_display_rotation_t sw_rotation)
    : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y,
                    mirror_x, mirror_y, swap_xy, fonts, sw_rotation) {
    {
        DisplayLockGuard lock(this);
        if (emotion_label_ != nullptr) {
            face_parent_ = lv_obj_get_parent(emotion_label_);
            lv_obj_add_flag(emotion_label_, LV_OBJ_FLAG_HIDDEN);

            img_ = lv_image_create(face_parent_);
            lv_obj_set_style_bg_opa(img_, LV_OPA_TRANSP, 0);
            LayOutFaceAndText();
            ApplyTrueBlack();
        }
    }

    if (img_ == nullptr) {
        ESP_LOGW(TAG, "No emotion label to replace, keeping the glyph UI");
    } else {
        ESP_LOGI(TAG, "Faces: %d emotions, %dx%d RGB565 from flash, 0 decoder RAM",
                 FACE_ANIM_COUNT, FACE_SIZE_PX, FACE_SIZE_PX);
        SetEmotion("neutral");
    }
}

// The content area is a vertical flex box holding the emoji label, the preview
// image and the chat text, centred as one group — which stacks the face and the
// text together wherever flex decides. Take both out of that layout and place
// them absolutely instead: the face in the middle of the content area, the chat
// text along the bottom edge.
void FaceDisplay::LayOutFaceAndText() {
    lv_obj_add_flag(img_, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_align(img_, LV_ALIGN_CENTER, 0, 0);

    if (chat_message_label_ == nullptr) {
        return;
    }
    lv_obj_add_flag(chat_message_label_, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_align(chat_message_label_, LV_ALIGN_BOTTOM_MID, 0, 0);

    // Give the text only the whole lines that fit between the bottom of the
    // centred face and the bottom edge, and ellipsise past that, so a long
    // reply grows no further and can never creep up over the face.
    lv_obj_update_layout(face_parent_);
    int line_height = fonts_.text_font->line_height;
    int room = (lv_obj_get_content_height(face_parent_) - FACE_SIZE_PX) / 2 - FACE_TEXT_GAP_PX;
    int lines = room / line_height;
    if (lines < 1) {
        return;  // no room below the face; leave the label as the stock UI had it
    }
    lv_obj_set_height(chat_message_label_, lines * line_height);
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_DOT);
}

// The stock dark theme is Material grey, not black: #121212 behind the status
// bar, #1E1E1E behind the face, and SetupUI leaves the LVGL card border on the
// content area in #333333. On a backlit IPS panel that reads as washed-out
// grey, and the face frames are pure #000000, so the face sat in a visibly
// lighter box. Black all of it out: the frames then blend into the screen and
// it is as dark as this panel can go. Light theme is left alone.
void FaceDisplay::ApplyTrueBlack() {
    if (current_theme_name_ != "dark" && current_theme_name_ != "DARK") {
        ESP_LOGI(TAG, "Theme '%s': leaving colours as the theme defines them",
                 current_theme_name_.c_str());
        return;
    }
    ESP_LOGI(TAG, "Theme 'dark': forcing true black so the faces blend in");
    const lv_color_t black = lv_color_black();
    current_theme_.background = black;
    current_theme_.chat_background = black;
    current_theme_.border = black;

    lv_obj_set_style_bg_color(lv_screen_active(), black, 0);
    if (container_ != nullptr) {
        lv_obj_set_style_bg_color(container_, black, 0);
        lv_obj_set_style_border_color(container_, black, 0);
    }
    if (status_bar_ != nullptr) {
        lv_obj_set_style_bg_color(status_bar_, black, 0);
    }
    if (face_parent_ != nullptr) {  // the content area the face and text sit on
        lv_obj_set_style_bg_color(face_parent_, black, 0);
        lv_obj_set_style_border_width(face_parent_, 0, 0);
    }
}

// The server can switch themes at runtime (self.screen.set_theme), which
// re-applies the stock colours, so black them out again afterwards.
void FaceDisplay::SetTheme(const std::string& theme_name) {
    SpiLcdDisplay::SetTheme(theme_name);
    DisplayLockGuard lock(this);
    ApplyTrueBlack();
}

void FaceDisplay::ShowFrame(uint8_t index) {
    if (index >= frame_count_) {
        return;
    }
    frame_index_ = index;
    lv_image_set_src(img_, &frames_[index]);
}

void FaceDisplay::TickCb(lv_timer_t* timer) {
    // Runs in the LVGL task, which already holds the port lock.
    auto* self = static_cast<FaceDisplay*>(lv_timer_get_user_data(timer));
    if (self == nullptr || self->frame_count_ < 2) {
        return;
    }
    self->ShowFrame((self->frame_index_ + 1) % self->frame_count_);
}

void FaceDisplay::SetEmotion(const char* emotion) {
    if (img_ == nullptr) {
        SpiLcdDisplay::SetEmotion(emotion);
        return;
    }

    const face_anim_t* anim = FindFace(emotion);
    if (anim == nullptr) {
        ESP_LOGD(TAG, "No face for '%s', using neutral", emotion ? emotion : "(null)");
        anim = FindFace("neutral");
    }
    if (anim == nullptr) {
        SpiLcdDisplay::SetEmotion(emotion);
        return;
    }
    if (current_emotion_ == anim->emotion) {
        return;  // already showing this face
    }

    DisplayLockGuard lock(this);

    frame_count_ = anim->frame_count > FACE_MAX_FRAMES ? FACE_MAX_FRAMES : anim->frame_count;
    for (uint8_t i = 0; i < frame_count_; i++) {
        lv_image_dsc_t& dsc = frames_[i];
        dsc = {};
        dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
        dsc.header.cf = LV_COLOR_FORMAT_RGB565;
        dsc.header.w = FACE_SIZE_PX;
        dsc.header.h = FACE_SIZE_PX;
        dsc.header.stride = FACE_SIZE_PX * 2;
        dsc.data = anim->frames[i];
        dsc.data_size = FACE_FRAME_BYTES;
    }

    ShowFrame(0);

    if (frame_count_ > 1) {
        if (timer_ == nullptr) {
            timer_ = lv_timer_create(TickCb, anim->period_ms, this);
        } else {
            lv_timer_set_period(timer_, anim->period_ms);
            lv_timer_resume(timer_);
        }
    } else if (timer_ != nullptr) {
        lv_timer_pause(timer_);
    }

    current_emotion_ = anim->emotion;
    ESP_LOGI(TAG, "Face '%s': %d frames @ %d ms", anim->emotion, frame_count_, anim->period_ms);
}
