#ifndef WIFI_SERVICE_H
#define WIFI_SERVICE_H

#include <Arduino.h>
#include <WiFi.h>

class WifiService
{
 public:
  bool connectStation(const char* ssid, const char* password,
                      uint32_t timeoutMs = 30000);
};

#endif