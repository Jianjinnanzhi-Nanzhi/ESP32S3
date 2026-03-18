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
  explicit CameraService(ResourceMutex& mutex);
  esp_err_t begin();
  esp_err_t captureAndSave();

 private:
  esp_err_t savePhoto(camera_fb_t* fb);

  ResourceMutex& mutex_;
  camera_config_t config_;
};

#endif