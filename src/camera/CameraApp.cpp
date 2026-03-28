#include "camera/CameraApp.h"

#include <Arduino.h>
#include <WiFi.h>

#include "LogSwitch.h"
#include "camera/CameraService.h"
#include "camera/MemoryPhotoStore.h"
#include "camera/PhotoWebServer.h"
#include "camera/WifiService.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace camera_app
{
const char* ssid = "桂花糕";
const char* password = "asdfghjkl";
static const uint16_t WEB_SERVER_PORT = 80;
static const IPAddress DEVICE_STATIC_IP(192, 168, 1, 188);
static const IPAddress DEVICE_GATEWAY(192, 168, 1, 1);
static const IPAddress DEVICE_SUBNET(255, 255, 255, 0);
static const IPAddress DEVICE_DNS1(8, 8, 8, 8);

static CameraService g_camera;
static MemoryPhotoStore g_photoStore(6);
static PhotoWebServer g_photoWeb(g_photoStore, &g_camera);
static WifiService g_wifi;

static const uint32_t CAPTURE_INTERVAL_MS = 34;
static const uint32_t WEB_TASK_STACK = 8192;
static const uint32_t CAPTURE_TASK_STACK = 8192;

static bool s_initialized = false;

static void webTask(void* pvParameters)
{
  LOG_PRINTLN(LOG_CAMERA, "WebTask: starting...");
  if (!g_wifi.connectStationStatic(ssid, password, DEVICE_STATIC_IP,
                                   DEVICE_GATEWAY, DEVICE_SUBNET, DEVICE_DNS1))
  {
    LOG_PRINTLN(LOG_CAMERA, "WebTask: WiFi connect failed");
    vTaskDelete(NULL);
    return;
  }

  if (!g_photoWeb.begin(WEB_SERVER_PORT))
  {
    LOG_PRINTLN(LOG_CAMERA, "WebTask: web server start failed");
    vTaskDelete(NULL);
    return;
  }

  for (;;)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

static void captureTask(void* pvParameters)
{
  for (;;)
  {
    uint8_t* buf = NULL;
    size_t len = 0;
    uint64_t tsMs = 0;

    if (g_camera.captureToJpegBuffer(&buf, &len, &tsMs) != ESP_OK)
    {
      LOG_PRINTLN(LOG_CAMERA, "CaptureTask: capture failed");
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }

    if (!g_photoStore.pushOwnedFrame(buf, len, tsMs))
    {
      LOG_PRINTLN(LOG_CAMERA, "CaptureTask: ring buffer busy, drop frame");
      free(buf);
    }
    else
    {
      g_photoWeb.notifyNewFrame();
    }

    vTaskDelay(pdMS_TO_TICKS(CAPTURE_INTERVAL_MS));
  }
}
}  // namespace camera_app

bool initCameraModule()
{
  if (!camera_app::g_photoStore.begin())
  {
    LOG_PRINTLN(LOG_CAMERA, "Photo store init failed");
    return false;
  }

  if (camera_app::g_camera.begin() != ESP_OK)
  {
    LOG_PRINTLN(LOG_CAMERA, "Camera init failed");
    return false;
  }

  LOG_PRINTLN(LOG_CAMERA, "Camera init success");
  camera_app::s_initialized = true;
  return true;
}

bool startCameraTasks()
{
  if (!camera_app::s_initialized)
  {
    LOG_PRINTLN(LOG_CAMERA, "camera init required before start tasks");
    return false;
  }

  BaseType_t webOk =
      xTaskCreatePinnedToCore(camera_app::webTask, "WebTask",
                              camera_app::WEB_TASK_STACK, NULL, 1, NULL, 0);
  BaseType_t capOk =
      xTaskCreatePinnedToCore(camera_app::captureTask, "CaptureTask",
                              camera_app::CAPTURE_TASK_STACK, NULL, 1, NULL, 1);

  if (webOk != pdPASS)
  {
    LOG_PRINTLN(LOG_CAMERA, "camera WebTask create failed");
  }
  if (capOk != pdPASS)
  {
    LOG_PRINTLN(LOG_CAMERA, "camera CaptureTask create failed");
  }
  if (webOk != pdPASS || capOk != pdPASS)
  {
    LOG_PRINTF(LOG_CAMERA, "camera free heap: %u\n", ESP.getFreeHeap());
    LOG_PRINTF(LOG_CAMERA, "camera min free heap: %u\n", ESP.getMinFreeHeap());
    return false;
  }

  return true;
}
