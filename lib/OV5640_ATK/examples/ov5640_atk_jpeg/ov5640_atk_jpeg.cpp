#include <Arduino.h>
#include <Wire.h>

// Optional (for capture):
// #include "esp_camera.h"

#include "OV5640_ATK.h"

// You said wiring is defined by you.
// Fill in your SCCB/I2C pins here.
static constexpr int PIN_SIOD = -1;  // SDA
static constexpr int PIN_SIOC = -1;  // SCL

// If you also use esp_camera, make sure these match the
// camera_config_t.sccb_sda/sccb_scl pins.

void setup()
{
  Serial.begin(115200);
  delay(200);

  Wire.begin(PIN_SIOD, PIN_SIOC, 400000);

  uint16_t id = ov5640_atk::readChipId(Wire);
  Serial.printf("OV5640 chip id: 0x%04X (expect 0x%04X)\n", id,
                ov5640_atk::kChipIdExpected);

  if (id != ov5640_atk::kChipIdExpected)
  {
    Serial.println("Chip ID mismatch. Check wiring / power / I2C addr.");
    while (true) delay(1000);
  }

  Serial.println("Applying init table...");
  if (!ov5640_atk::applyInit(Wire))
  {
    Serial.println("applyInit failed");
    while (true) delay(1000);
  }

  Serial.println("Switching to JPEG table...");
  if (!ov5640_atk::applyJpeg(Wire))
  {
    Serial.println("applyJpeg failed");
    while (true) delay(1000);
  }

  // Optional: autofocus firmware download (takes some time)
  // Serial.println("Downloading AF firmware...");
  // if (!ov5640_atk::autofocusDownload(Wire)) {
  //   Serial.println("autofocusDownload failed/timeout");
  // } else {
  //   Serial.println("AF firmware ready, starting continuous AF...");
  //   ov5640_atk::autofocusContinuous(Wire);
  // }

  Serial.println(
      "OV5640 registers configured. Next: init capture pipeline "
      "(esp_camera or your own). ");
}

void loop()
{
  // This example only configures the sensor over I2C.
  // Capturing frames on ESP32-S3 is typically done via esp32-camera
  // (esp_camera.h).
  delay(1000);
}
