#ifndef WIFI_CREDENTIAL_STORE_H
#define WIFI_CREDENTIAL_STORE_H

#include <Arduino.h>

class WifiCredentialStore
{
 public:
  bool save(const char* ssid, const char* password);
  bool load(String& ssid, String& password);
  bool clear();
  bool hasSaved();
};

#endif
