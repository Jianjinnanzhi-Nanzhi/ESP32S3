#include "WIFI/WifiCredentialStore.h"

#include <Preferences.h>

namespace
{
static const char* WIFI_PREF_NAMESPACE = "wifi_cfg";
static const char* WIFI_PREF_KEY_SSID = "ssid";
static const char* WIFI_PREF_KEY_PASS = "pass";
}  // namespace

bool WifiCredentialStore::save(const char* ssid, const char* password)
{
  if (ssid == nullptr || password == nullptr || strlen(ssid) == 0)
  {
    return false;
  }

  Preferences pref;
  if (!pref.begin(WIFI_PREF_NAMESPACE, false))
  {
    return false;
  }

  size_t ssidWritten = pref.putString(WIFI_PREF_KEY_SSID, ssid);
  pref.putString(WIFI_PREF_KEY_PASS, password);
  pref.end();

  return ssidWritten > 0;
}

bool WifiCredentialStore::load(String& ssid, String& password)
{
  Preferences pref;
  if (!pref.begin(WIFI_PREF_NAMESPACE, true))
  {
    return false;
  }

  ssid = pref.getString(WIFI_PREF_KEY_SSID, "");
  password = pref.getString(WIFI_PREF_KEY_PASS, "");
  pref.end();

  return ssid.length() > 0;
}

bool WifiCredentialStore::clear()
{
  Preferences pref;
  if (!pref.begin(WIFI_PREF_NAMESPACE, false))
  {
    return false;
  }

  bool removedSsid = pref.remove(WIFI_PREF_KEY_SSID);
  bool removedPass = pref.remove(WIFI_PREF_KEY_PASS);
  pref.end();

  return removedSsid || removedPass;
}

bool WifiCredentialStore::hasSaved()
{
  String ssid;
  String password;
  return load(ssid, password);
}
