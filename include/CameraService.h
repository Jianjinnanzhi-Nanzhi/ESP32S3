#ifndef CAMERA_SERVICE_H
#define CAMERA_SERVICE_H

#include <Arduino.h>

#include "LittleFS.h"
#include "ResourceMutex.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "esp_timer.h"

class CameraService
{
 public:
  CameraService();
  esp_err_t begin();
  esp_err_t captureToJpegBuffer(uint8_t** outBuf, size_t* outLen,
                                uint64_t* outTsMs);
  esp_err_t saveJpegBuffer(const uint8_t* buf, size_t len, uint64_t tsMs);
  esp_err_t captureAndSave();

 private:
  esp_err_t savePhotoFrame(camera_fb_t* fb);
  camera_config_t config_;
};

#endif