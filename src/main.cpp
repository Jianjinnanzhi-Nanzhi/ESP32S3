#include <Arduino.h>

#include "LogSwitch.h"
#include "WIFI/WifiProvisioning.h"
#include "ble/BleApp.h"
#include "camera/CameraApp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "motor/MotorBleApp.h"
#include "ultrasonic/ultrasonic.h"

namespace app_boot
{
void forwardWifiNotifyToBle(const String& message) { bleSendMessage(message); }
bool handleWifiBleBridgeCommand(const String& data)
{
  return handleWifiBleCommand(data);
}
}  // namespace app_boot

void setup()
{
  Serial.begin(115200);
  delay(1000);

  // 1) 初始化所有模块
  bool motorInitOk = initMotorBleApp();
  bool bleInitOk = initBleApp();
  bool wifiInitOk =
      initWifiProvisioningModule(app_boot::forwardWifiNotifyToBle);
  bool ultrasonicInitOk = ultrasonic::initUltrasonicModule(ULTRASONIC_TRIG_PIN,
                                                           ULTRASONIC_ECHO_PIN);
  bool cameraInitOk = initCameraModule();

  bleRegisterExternalCommandHandler(app_boot::handleWifiBleBridgeCommand);

  // 2) 统一启动各模块任务
  bool motorTaskOk = false;
  bool bleTaskOk = false;
  bool wifiTaskOk = false;
  bool ultrasonicTaskOk = false;
  bool cameraTaskOk = false;

  if (motorInitOk)
  {
    motorTaskOk = startMotorBleTasks();
  }
  if (bleInitOk)
  {
    bleTaskOk = startBleTasks();
  }
  if (wifiInitOk)
  {
    wifiTaskOk = startWifiProvisioningTask(1, 0, 4096);
  }
  if (ultrasonicInitOk)
  {
    ultrasonicTaskOk = ultrasonic::startUltrasonicTask();
  }
  if (cameraInitOk)
  {
    cameraTaskOk = startCameraTasks();
  }

  if (!motorInitOk || !bleInitOk || !wifiInitOk || !ultrasonicInitOk ||
      !cameraInitOk || !motorTaskOk || !bleTaskOk || !wifiTaskOk ||
      !ultrasonicTaskOk || !cameraTaskOk)
  {
    LOG_PRINTLN(LOG_CAMERA, "[Main] module init/start has failures");
  }
}

void loop()
{
  // 避免空转占满 CPU，保证系统任务调度稳定
  vTaskDelay(pdMS_TO_TICKS(1000));
}
