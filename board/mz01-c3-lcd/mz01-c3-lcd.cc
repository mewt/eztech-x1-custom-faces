#include "wifi_board.h"
#include "audio/codecs/vb6824_audio_codec.h"
#include "display/lcd_display.h"
#include "face_display.h"
#include "power_manager.h"
#include "application.h"
#include "button.h"
#include "config.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/spi_common.h>
#include <driver/gpio.h>

#include <wifi_station.h>
#include <wifi_configuration_ap.h>
#include <ssid_manager.h>

#include "font_awesome_symbols.h"
#include "settings.h"
#include "assets/lang_config.h"

#define TAG "Mz01C3Lcd"

LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_awesome_14_1);

class CustomBoard : public WifiBoard {
private:
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    VbAduioCodec audio_codec;
    FaceDisplay* display = nullptr;
    PowerManager* power_manager_ = nullptr;

    // The board is held on by a latch on POWER_HOLD_PIN. Drive it high before
    // anything else or the device cuts its own power as soon as the button is
    // released.
    static void LatchPower() {
        gpio_config_t hold_conf = {
            .pin_bit_mask = (1ULL << POWER_HOLD_PIN),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&hold_conf);
        gpio_set_level(POWER_HOLD_PIN, 1);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            if (audio_codec.InOtaMode(1) == true) {
                ESP_LOGI(TAG, "OTA mode, do not enter chat");
                return;
            }
            auto& app = Application::GetInstance();
            app.ToggleChatState();
        });
        boot_button_.OnPressRepeat([this](uint16_t count) {
            if (count >= 3 && audio_codec.InOtaMode(1) == false) {
                ResetWifiConfiguration();
            }
        });

        volume_up_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });
        volume_up_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });
        volume_down_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });
        volume_down_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = 3;
        io_config.pclk_hz = DISPLAY_SPI_SCLK_HZ;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);

        // The panel stays in its NATIVE orientation: this ST7789 exposes a
        // 240x240 window inside 240x320 controller RAM, so MADCTL rotation
        // mirrors/wraps the framebuffer. LVGL rotates in software instead.
        display = new FaceDisplay(panel_io, panel,
                                  DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                  DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                  DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                  {
                                      .text_font = &font_puhui_14_1,
                                      .icon_font = &font_awesome_14_1,
                                      .emoji_font = font_emoji_32_init(),
                                  },
                                  LV_DISPLAY_ROTATION_90);
    }

public:
    CustomBoard()
        : boot_button_(BOOT_BUTTON_GPIO),
          volume_up_button_(VOLUME_UP_BUTTON_GPIO),
          volume_down_button_(VOLUME_DOWN_BUTTON_GPIO),
          audio_codec(CODEC_TX_GPIO, CODEC_RX_GPIO) {
        LatchPower();
        power_manager_ = new PowerManager(POWER_CHARGE_PIN);
        InitializeButtons();
        InitializeSpi();
        InitializeLcdDisplay();
        GetBacklight()->RestoreBrightness();
        audio_codec.OnWakeUp([this](const std::string& command) {
            if (command == std::string(vb6824_get_wakeup_word())) {
                if (Application::GetInstance().GetDeviceState() != kDeviceStateListening) {
                    Application::GetInstance().WakeWordInvoke(vb6824_get_wakeup_word());
                }
            } else if (command == "开始配网") {
                ResetWifiConfiguration();
            }
        });
    }

    virtual AudioCodec* GetAudioCodec() override {
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        if (power_manager_ == nullptr) {
            return false;
        }
        charging = power_manager_->IsCharging();
        discharging = !charging;
        level = power_manager_->GetBatteryLevel();
        return true;
    }
};

DECLARE_BOARD(CustomBoard);
