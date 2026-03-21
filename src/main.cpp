#include <Arduino.h>
#include <WiFi.h>

#include "CameraService.h"
#include "MemoryPhotoStore.h"
#include "PhotoWebServer.h"
#include "WifiService.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

const char* ssid = "桂花糕";
const char* password = "asdfghjkl";

static CameraService g_camera;
static MemoryPhotoStore g_photoStore(6);
static PhotoWebServer g_photoWeb(g_photoStore);
static WifiService g_wifi;
static const uint32_t CAPTURE_INTERVAL_MS = 100;
static const uint32_t WEB_TASK_STACK = 8192;
static const uint32_t CAPTURE_TASK_STACK = 163840;

static void webTask(void* pvParameters)
{
  Serial.println("WebTask: starting...");
  if (!g_wifi.connectStation(ssid, password))
  {
    Serial.println("WebTask: WiFi connect failed");
    vTaskDelete(NULL);
    return;
  }

  if (!g_photoWeb.begin(80))
  {
    Serial.println("WebTask: web server start failed");
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
      Serial.println("CaptureTask: capture failed");
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }

    if (!g_photoStore.pushOwnedFrame(buf, len, tsMs))
    {
      Serial.println("CaptureTask: ring buffer busy, drop frame");
      free(buf);
    }
    else
    {
      g_photoWeb.notifyNewFrame();
    }

    vTaskDelay(pdMS_TO_TICKS(CAPTURE_INTERVAL_MS));
  }
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  if (!g_photoStore.begin())
  {
    Serial.println("Photo store init failed");
    return;
  }

  // 初始化摄像头
  if (g_camera.begin() != ESP_OK)
  {
    Serial.println("Camera init failed");
    return;
  }

  BaseType_t webOk = xTaskCreatePinnedToCore(webTask, "WebTask", WEB_TASK_STACK,
                                             NULL, 2, NULL, 0);
  BaseType_t capOk = xTaskCreatePinnedToCore(
      captureTask, "CaptureTask", CAPTURE_TASK_STACK, NULL, 1, NULL, 1);

  if (webOk != pdPASS || capOk != pdPASS)
  {
    Serial.println("Task create failed");
  }
}

void loop()
{
  // 避免空转占满 CPU，保证系统任务调度稳定
  vTaskDelay(pdMS_TO_TICKS(1000));
}
