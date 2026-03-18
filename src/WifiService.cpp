#include "WifiService.h"

bool WifiService::connectStation(const char* ssid, const char* password,
                                 uint32_t timeoutMs)
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("正在连接 WiFi: ");
  Serial.println(ssid);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    if (millis() - start >= timeoutMs)
    {
      Serial.println("\nWiFi 连接超时");
      return false;
    }
  }

  Serial.println("\nWiFi 连接成功!");
  Serial.print("ESP32 IP 访问地址: ");
  Serial.println(WiFi.localIP());
  return true;
}