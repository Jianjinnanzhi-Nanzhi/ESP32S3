#include "CameraService.h"

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

CameraService::CameraService(ResourceMutex& mutex) : mutex_(mutex)
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

  config_.xclk_freq_hz = 10000000;
  config_.ledc_timer = LEDC_TIMER_0;
  config_.ledc_channel = LEDC_CHANNEL_0;

  config_.pixel_format = PIXFORMAT_JPEG;
  config_.frame_size = FRAMESIZE_QVGA;
  config_.jpeg_quality = 12;
  config_.fb_count = 1;
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
  return ESP_OK;
}

esp_err_t CameraService::captureAndSave()
{
  if (!mutex_.lock(2000))
  {
    ESP_LOGW(CAMERA_TAG, "camera/fs busy, skip one capture");
    return ESP_FAIL;
  }

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

    result = savePhoto(fb);
    esp_camera_fb_return(fb);
    if (result == ESP_OK)
    {
      break;
    }
  }

  mutex_.unlock();
  return result;
}

esp_err_t CameraService::savePhoto(camera_fb_t* fb)
{
  if (fb == NULL)
  {
    return ESP_FAIL;
  }
  if (fb->format != PIXFORMAT_JPEG)
  {
    ESP_LOGE(CAMERA_TAG, "Frame is not JPEG");
    return ESP_FAIL;
  }

  char path[64];
  uint64_t ts = (uint64_t)(esp_timer_get_time() / 1000ULL);
  snprintf(path, sizeof(path), "/picture_%llu.jpg", (unsigned long long)ts);

  File file = LittleFS.open(path, FILE_WRITE);
  if (!file)
  {
    ESP_LOGE(CAMERA_TAG, "Create file failed: %s", path);
    return ESP_FAIL;
  }

  file.write(fb->buf, fb->len);
  file.close();
  ESP_LOGI(CAMERA_TAG, "Saved: %s (%u bytes)", path, (unsigned int)fb->len);
  return ESP_OK;
}