#include <Arduino.h>

#include "LogSwitch.h"
#include "camera/CameraApp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "motor/MotorBleApp.h"
#include "ultrasonic/ultrasonic.h"

void setup()
{
  Serial.begin(115200);
  delay(1000);

  // 1) 初始化所有模块（不创建任务）
  bool motorInitOk = initMotorBleApp();
  bool ultrasonicInitOk = ultrasonic::initUltrasonicModule(ULTRASONIC_TRIG_PIN,
                                                           ULTRASONIC_ECHO_PIN);
  bool cameraInitOk = initCameraModule();

  // 2) 统一启动各模块任务
  bool motorTaskOk = false;
  bool ultrasonicTaskOk = false;
  bool cameraTaskOk = false;

  if (motorInitOk)
  {
    motorTaskOk = startMotorBleTasks();
  }
  if (ultrasonicInitOk)
  {
    ultrasonicTaskOk = ultrasonic::startUltrasonicTask();
  }
  if (cameraInitOk)
  {
    cameraTaskOk = startCameraTasks();
  }

  if (!motorInitOk || !ultrasonicInitOk || !cameraInitOk || !motorTaskOk ||
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
