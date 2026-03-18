#include <Arduino.h>
#include <WiFi.h>

#include "CameraService.h"
#include "LittleFS.h"
#include "PhotoWebServer.h"
#include "ResourceMutex.h"
#include "WifiService.h"

const char* ssid = "Redmi";
const char* password = "88889999";

static ResourceMutex g_resourceMutex;
static CameraService g_camera(g_resourceMutex);
static PhotoWebServer g_photoWeb(g_resourceMutex);
static WifiService g_wifi;

void setup()
{
  Serial.begin(115200);
  delay(1000);

  // 初始化文件系统
  if (!LittleFS.begin(true))
  {
    Serial.println("LittleFS Mount Failed");
    return;
  }

  if (!g_resourceMutex.begin())
  {
    Serial.println("Resource mutex init failed");
    return;
  }

  // 初始化摄像头
  if (g_camera.begin() != ESP_OK)
  {
    Serial.println("Camera init failed");
    return;
  }

  if (!g_wifi.connectStation(ssid, password))
  {
    return;
  }

  g_photoWeb.begin(80);
}

void loop()
{
  // 每隔10秒拍一张照片
  Serial.println("拍摄一张新照片...");
  g_camera.captureAndSave();
  delay(10000);
}
