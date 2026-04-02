#ifndef WIFI_PROVISIONING_H
#define WIFI_PROVISIONING_H

#include <Arduino.h>

typedef void (*WifiNotifyCallback)(const String& message);

bool initWifiProvisioningModule(WifiNotifyCallback notifyCb);
bool startWifiProvisioningTask(UBaseType_t priority = 1, BaseType_t core = 0,
                               uint32_t stackWords = 4096);
bool handleWifiBleCommand(const String& data);
bool isWifiProvisionedConnected();

#endif
