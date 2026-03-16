#include <Arduino.h>
#include <Wire.h>

#include "OV5640_ATK.h"
#include "esp_camera.h"

// Wiring is defined by you: fill ALL pins below.
// OV5640 is 8-bit parallel (D0..D7) + PCLK/VSYNC/HREF + XCLK + SCCB(SIOD/SIOC).
static constexpr int PIN_PWDN = -1;
static constexpr int PIN_RESET = -1;
static constexpr int PIN_XCLK = -1;
static constexpr int PIN_SIOD = -1;  // SCCB SDA
static constexpr int PIN_SIOC = -1;  // SCCB SCL

static constexpr int PIN_D0 = -1;
static constexpr int PIN_D1 = -1;
static constexpr int PIN_D2 = -1;
static constexpr int PIN_D3 = -1;
static constexpr int PIN_D4 = -1;
static constexpr int PIN_D5 = -1;
static constexpr int PIN_D6 = -1;
static constexpr int PIN_D7 = -1;
static constexpr int PIN_VSYNC = -1;
static constexpr int PIN_HREF = -1;
static constexpr int PIN_PCLK = -1;

static camera_config_t make_config()
{
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;

  config.pin_d0 = PIN_D0;
  config.pin_d1 = PIN_D1;
  config.pin_d2 = PIN_D2;
  config.pin_d3 = PIN_D3;
  config.pin_d4 = PIN_D4;
  config.pin_d5 = PIN_D5;
  config.pin_d6 = PIN_D6;
  config.pin_d7 = PIN_D7;
  config.pin_xclk = PIN_XCLK;
  config.pin_pclk = PIN_PCLK;
  config.pin_vsync = PIN_VSYNC;
  config.pin_href = PIN_HREF;
  config.pin_sccb_sda = PIN_SIOD;
  config.pin_sccb_scl = PIN_SIOC;
  config.pin_pwdn = PIN_PWDN;
  config.pin_reset = PIN_RESET;

  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_QVGA;
  config.pixel_format = PIXFORMAT_JPEG;

  // Start conservative; increase if you have PSRAM and stable wiring.
  config.jpeg_quality = 12;
  config.fb_count = 1;
  config.fb_location =
      CAMERA_FB_IN_PSRAM;  // if no PSRAM, change to CAMERA_FB_IN_DRAM
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  return config;
}

void setup()
{
  Serial.begin(115200);
  delay(200);

  camera_config_t config = make_config();
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("esp_camera_init failed: 0x%x\n", (unsigned)err);
    while (true) delay(1000);
  }

  // Use Wire on the same SCCB pins. (esp_camera also uses SCCB internally.)
  // If you see SCCB/I2C instability, comment Wire out and rely on esp_camera's
  // sensor API instead.
  Wire.begin(PIN_SIOD, PIN_SIOC, 400000);

  uint16_t id = ov5640_atk::readChipId(Wire);
  Serial.printf("OV5640 chip id: 0x%04X\n", id);

  // Apply ATK tables on top of esp_camera init (optional).
  // If your esp_camera build already has OV5640 support, you might not need
  // these.
  ov5640_atk::applyInit(Wire);
  ov5640_atk::applyJpeg(Wire);

  Serial.println("Camera ready.");
}

void loop()
{
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb)
  {
    Serial.println("esp_camera_fb_get failed");
    delay(1000);
    return;
  }

  Serial.printf("Got frame: %ux%u, len=%u, format=%d\n", fb->width, fb->height,
                (unsigned)fb->len, (int)fb->format);

  esp_camera_fb_return(fb);
  delay(1000);
}
