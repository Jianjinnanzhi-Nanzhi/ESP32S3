#ifndef BLE_APP_H
#define BLE_APP_H

#include <Arduino.h>

typedef bool (*BleExternalCommandHandler)(const String& data);

bool initBleApp();
bool startBleTasks();
void bleSendMessage(const String& message);
void bleRegisterExternalCommandHandler(BleExternalCommandHandler handler);

#endif
