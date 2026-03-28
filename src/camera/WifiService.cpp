#include "camera/WifiService.h"

#include "LogSwitch.h"


bool WifiService::connectStation(const char* ssid, const char* password,
                                 uint32_t timeoutMs)
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  LOG_PRINT(LOG_CAMERA, "正在连接 WiFi: ");
  LOG_PRINTLN(LOG_CAMERA, ssid);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    LOG_PRINT(LOG_CAMERA, ".");
    if (millis() - start >= timeoutMs)
    {
      LOG_PRINTLN(LOG_CAMERA, "\nWiFi 连接超时");
      return false;
    }
  }

  LOG_PRINTLN(LOG_CAMERA, "\nWiFi 连接成功!");
  LOG_PRINT(LOG_CAMERA, "ESP32 IP 访问地址: ");
  LOG_PRINTLN(LOG_CAMERA, WiFi.localIP());
  return true;
}

bool WifiService::connectStationStatic(const char* ssid, const char* password,
                                       const IPAddress& localIp,
                                       const IPAddress& gateway,
                                       const IPAddress& subnet,
                                       const IPAddress& dns1,
                                       uint32_t timeoutMs)
{
  WiFi.mode(WIFI_STA);
  if (!WiFi.config(localIp, gateway, subnet, dns1))
  {
    LOG_PRINTLN(LOG_CAMERA, "WiFi static IP config failed");
    return false;
  }

  WiFi.begin(ssid, password);

  LOG_PRINT(LOG_CAMERA, "正在连接 WiFi(静态IP): ");
  LOG_PRINTLN(LOG_CAMERA, ssid);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    LOG_PRINT(LOG_CAMERA, ".");
    if (millis() - start >= timeoutMs)
    {
      LOG_PRINTLN(LOG_CAMERA, "\nWiFi 连接超时");
      return false;
    }
  }

  LOG_PRINTLN(LOG_CAMERA, "\nWiFi 连接成功!");
  LOG_PRINT(LOG_CAMERA, "ESP32 固定 IP 地址: ");
  LOG_PRINTLN(LOG_CAMERA, WiFi.localIP());
  return true;
}