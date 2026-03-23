#include "CameraService.h"

#include <string.h>

// 适配当前 ESP32-S3 OV5640 板卡引脚
#define CAM_PIN_PWDN -1
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK 15
#define CAM_PIN_SIOD 4
#define CAM_PIN_SIOC 5

#define CAM_PIN_D7 16
#define CAM_PIN_D6 17
#define CAM_PIN_D5 18
#define CAM_PIN_D4 12
#define CAM_PIN_D3 10
#define CAM_PIN_D2 8
#define CAM_PIN_D1 9
#define CAM_PIN_D0 11
#define CAM_PIN_VSYNC 6
#define CAM_PIN_HREF 7
#define CAM_PIN_PCLK 13

static const char* CAMERA_TAG = "CameraService";

static bool isLikelyValidJpegFrame(const camera_fb_t* fb)
{
  if (fb == NULL || fb->buf == NULL || fb->len < 4)
  {
    return false;
  }

  const uint8_t* data = fb->buf;
  size_t len = fb->len;
  return data[0] == 0xFF && data[1] == 0xD8 && data[len - 2] == 0xFF &&
         data[len - 1] == 0xD9;
}

CameraService::CameraService()
{
  config_.pin_pwdn = CAM_PIN_PWDN;
  config_.pin_reset = CAM_PIN_RESET;
  config_.pin_xclk = CAM_PIN_XCLK;
  config_.pin_sccb_sda = CAM_PIN_SIOD;
  config_.pin_sccb_scl = CAM_PIN_SIOC;

  config_.pin_d7 = CAM_PIN_D7;
  config_.pin_d6 = CAM_PIN_D6;
  config_.pin_d5 = CAM_PIN_D5;
  config_.pin_d4 = CAM_PIN_D4;
  config_.pin_d3 = CAM_PIN_D3;
  config_.pin_d2 = CAM_PIN_D2;
  config_.pin_d1 = CAM_PIN_D1;
  config_.pin_d0 = CAM_PIN_D0;
  config_.pin_vsync = CAM_PIN_VSYNC;
  config_.pin_href = CAM_PIN_HREF;
  config_.pin_pclk = CAM_PIN_PCLK;

  config_.xclk_freq_hz = 20000000;
  config_.ledc_timer = LEDC_TIMER_0;
  config_.ledc_channel = LEDC_CHANNEL_0;

  config_.pixel_format = PIXFORMAT_JPEG;
  config_.frame_size = FRAMESIZE_VGA;
  config_.jpeg_quality = 7;
  config_.fb_count = 2;
  config_.grab_mode = CAMERA_GRAB_LATEST;
}

esp_err_t CameraService::begin()
{
  if (CAM_PIN_PWDN != -1)
  {
    pinMode(CAM_PIN_PWDN, OUTPUT);
    digitalWrite(CAM_PIN_PWDN, LOW);
  }

  esp_err_t err = esp_camera_init(&config_);
  if (err != ESP_OK)
  {
    ESP_LOGE(CAMERA_TAG, "Camera init failed");
    return err;
  }

  ESP_LOGI(CAMERA_TAG, "Camera init success");

  // 默认使用自动曝光，启动阶段避免首帧过暗/过曝。
  setAutoExposure(true);
  return ESP_OK;
}

esp_err_t CameraService::captureAndSave()
{
  uint8_t* buf = NULL;
  size_t len = 0;
  uint64_t tsMs = 0;

  esp_err_t cap = captureToJpegBuffer(&buf, &len, &tsMs);
  if (cap != ESP_OK)
  {
    return cap;
  }

  esp_err_t save = saveJpegBuffer(buf, len, tsMs);
  free(buf);
  return save;
}

esp_err_t CameraService::captureToJpegBuffer(uint8_t** outBuf, size_t* outLen,
                                             uint64_t* outTsMs)
{
  if (outBuf == NULL || outLen == NULL || outTsMs == NULL)
  {
    return ESP_FAIL;
  }

  *outBuf = NULL;
  *outLen = 0;
  *outTsMs = 0;

  esp_err_t result = ESP_FAIL;
  for (int attempt = 0; attempt < 3; ++attempt)
  {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb)
    {
      ESP_LOGW(CAMERA_TAG, "Camera capture failed, attempt %d/3", attempt + 1);
      vTaskDelay(pdMS_TO_TICKS(120 * (attempt + 1)));
      continue;
    }

    if (fb->format != PIXFORMAT_JPEG)
    {
      ESP_LOGE(CAMERA_TAG, "Frame is not JPEG");
      esp_camera_fb_return(fb);
      result = ESP_FAIL;
      break;
    }

    if (!isLikelyValidJpegFrame(fb))
    {
      ESP_LOGW(CAMERA_TAG, "Invalid JPEG markers, attempt %d/3", attempt + 1);
      esp_camera_fb_return(fb);
      vTaskDelay(pdMS_TO_TICKS(50 * (attempt + 1)));
      continue;
    }

    uint8_t* copied = (uint8_t*)malloc(fb->len);
    if (copied == NULL)
    {
      ESP_LOGE(CAMERA_TAG, "Malloc failed for frame copy (%u bytes)",
               (unsigned int)fb->len);
      esp_camera_fb_return(fb);
      result = ESP_ERR_NO_MEM;
      break;
    }

    memcpy(copied, fb->buf, fb->len);
    *outBuf = copied;
    *outLen = fb->len;
    *outTsMs = (uint64_t)(esp_timer_get_time() / 1000ULL);

    esp_camera_fb_return(fb);
    result = ESP_OK;
    break;
  }

  return result;
}

esp_err_t CameraService::saveJpegBuffer(const uint8_t* buf, size_t len,
                                        uint64_t tsMs)
{
  if (buf == NULL || len == 0)
  {
    return ESP_FAIL;
  }

  char path[64];
  snprintf(path, sizeof(path), "/picture_%llu.jpg", (unsigned long long)tsMs);

  File file = LittleFS.open(path, FILE_WRITE);
  if (!file)
  {
    ESP_LOGE(CAMERA_TAG, "Create file failed: %s", path);
    return ESP_FAIL;
  }

  file.write(buf, len);
  file.close();
  ESP_LOGI(CAMERA_TAG, "Saved: %s (%u bytes)", path, (unsigned int)len);
  return ESP_OK;
}

esp_err_t CameraService::savePhotoFrame(camera_fb_t* fb)
{
  if (fb == NULL)
  {
    return ESP_FAIL;
  }
  return saveJpegBuffer(fb->buf, fb->len,
                        (uint64_t)(esp_timer_get_time() / 1000ULL));
}

esp_err_t CameraService::setAutoExposure(bool enabled)
{
  sensor_t* sensor = esp_camera_sensor_get();
  if (sensor == NULL || sensor->set_exposure_ctrl == NULL)
  {
    ESP_LOGE(CAMERA_TAG, "Sensor not ready for auto exposure");
    return ESP_FAIL;
  }

  if (sensor->set_exposure_ctrl(sensor, enabled ? 1 : 0) != 0)
  {
    ESP_LOGE(CAMERA_TAG, "set_exposure_ctrl(%d) failed", enabled ? 1 : 0);
    return ESP_FAIL;
  }

  ESP_LOGI(CAMERA_TAG, "Auto exposure: %s", enabled ? "ON" : "OFF");
  return ESP_OK;
}

esp_err_t CameraService::setManualExposure(int value)
{
  sensor_t* sensor = esp_camera_sensor_get();
  if (sensor == NULL || sensor->set_exposure_ctrl == NULL ||
      sensor->set_aec_value == NULL)
  {
    ESP_LOGE(CAMERA_TAG, "Sensor not ready for manual exposure");
    return ESP_FAIL;
  }

  const int clamped = clampExposureValue(value);

  if (sensor->set_exposure_ctrl(sensor, 0) != 0)
  {
    ESP_LOGE(CAMERA_TAG, "Disable auto exposure failed");
    return ESP_FAIL;
  }

  if (sensor->set_aec_value(sensor, clamped) != 0)
  {
    ESP_LOGE(CAMERA_TAG, "set_aec_value(%d) failed", clamped);
    return ESP_FAIL;
  }

  ESP_LOGI(CAMERA_TAG, "Manual exposure set: %d", clamped);
  return ESP_OK;
}

esp_err_t CameraService::getExposureState(bool* outAutoEnabled, int* outValue)
{
  if (outAutoEnabled == NULL || outValue == NULL)
  {
    return ESP_FAIL;
  }

  sensor_t* sensor = esp_camera_sensor_get();
  if (sensor == NULL)
  {
    return ESP_FAIL;
  }

  *outAutoEnabled = sensor->status.aec != 0;
  *outValue = clampExposureValue((int)sensor->status.aec_value);
  return ESP_OK;
}

int CameraService::clampExposureValue(int value) const
{
  if (value < kExposureMin)
  {
    return kExposureMin;
  }
  if (value > kExposureMax)
  {
    return kExposureMax;
  }
  return value;
}