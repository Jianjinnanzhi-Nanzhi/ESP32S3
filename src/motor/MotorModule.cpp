#include "motor/MotorModule.h"

#include <Arduino.h>

#include "LogSwitch.h"
#include "ultrasonic/ultrasonic.h"

namespace esp32s3
{
#define MOTOR_PIN_1 39
#define MOTOR_PIN_2 40
#define MOTOR_PIN_3 41
#define MOTOR_PIN_4 42

typedef enum
{
  MOTOR_CMD_STOP,
  MOTOR_CMD_START,
} MotorCmdType;

typedef struct
{
  MotorCmdType cmd;
  int angle;
} MotorCommand;

QueueHandle_t motorCmdQueue = NULL;
bool motorRunning = false;
int motorTargetAngle = 0;
bool s_motorInitialized = false;

void calculateMotors(int angle, bool* m1, bool* m2, bool* m3, bool* m4)
{
  *m1 = (angle >= -60 && angle <= 60);
  *m2 = (angle >= 30 && angle <= 150);
  *m3 = (angle >= -120 && angle <= 120);
  *m4 = (angle >= -150 && angle <= -30);

  LOG_PRINTF(
      LOG_MOTOR,
      "[Motor] 角度: %d | 马达1: %s | 马达2: %s | 马达3: %s | 马达4: %s\n",
      angle, *m1 ? "ON" : "OFF", *m2 ? "ON" : "OFF", *m3 ? "ON" : "OFF",
      *m4 ? "ON" : "OFF");
}

void setMotors(bool m1, bool m2, bool m3, bool m4)
{
  digitalWrite(MOTOR_PIN_1, m1 ? HIGH : LOW);
  digitalWrite(MOTOR_PIN_2, m2 ? HIGH : LOW);
  digitalWrite(MOTOR_PIN_3, m3 ? HIGH : LOW);
  digitalWrite(MOTOR_PIN_4, m4 ? HIGH : LOW);
}

void stopAllMotors()
{
  setMotors(false, false, false, false);
  LOG_PRINTLN(LOG_MOTOR, "[Motor] 所有马达已停止");
}

void initMotorPins()
{
  pinMode(MOTOR_PIN_1, OUTPUT);
  pinMode(MOTOR_PIN_2, OUTPUT);
  pinMode(MOTOR_PIN_3, OUTPUT);
  pinMode(MOTOR_PIN_4, OUTPUT);
  stopAllMotors();
  LOG_PRINTLN(LOG_MOTOR, "[Motor] 马达引脚初始化完成");
}

void motorTask(void* pvParameters)
{
  MotorCommand motorCmd;
  bool motorState0 = false;//导航马达
  bool motorState1 = false;//超声波马达
  unsigned long motorLastToggle0 = 0;//导航马达
  unsigned long motorLastToggle1= 0;//超声波马达
  bool currentM1 = false;
  bool currentM2 = false;
  bool currentM3 = false;
  bool currentM4 = false;

  LOG_PRINTLN(LOG_MOTOR, "[Motor Task] 任务已启动, 运行在 Core 0");

  while (true)
  {
    if (xQueueReceive(motorCmdQueue, &motorCmd, 0) == pdPASS)
    {
      if (motorCmd.cmd == MOTOR_CMD_START)
      {
        motorRunning = true;
        motorTargetAngle = motorCmd.angle;
        motorState0 = true;
        motorLastToggle0 = millis();

        calculateMotors(motorTargetAngle, &currentM1, &currentM2, &currentM3,
                        &currentM4);
        setMotors(currentM1, currentM2, currentM3, currentM4);

        LOG_PRINTF(LOG_MOTOR, "[Motor] 开始振动, 目标角度: %d\n",
                   motorTargetAngle);
      }
      else if (motorCmd.cmd == MOTOR_CMD_STOP)
      {
        motorRunning = false;
        stopAllMotors();
        LOG_PRINTLN(LOG_MOTOR, "[Motor] 停止振动");
      }
    }

    if (motorRunning)
    {
      unsigned long now = millis();
      if (now - motorLastToggle0 >= 1000)
      {
        motorState0 = !motorState0;
        motorLastToggle0 = now;

        if (motorState0)
        {
          calculateMotors(motorTargetAngle, &currentM1, &currentM2, &currentM3,
                          &currentM4);
          setMotors(currentM1, currentM2, currentM3, currentM4);
          LOG_PRINTLN(LOG_MOTOR, "[Motor] 马达开启");
        }
        else
        {
          stopAllMotors();
          LOG_PRINTLN(LOG_MOTOR, "[Motor] 马达关闭");
        }
      }
      if (now - motorLastToggle1 >= 500)
      {
        motorState1 = !motorState1;
        motorLastToggle1 = now;

        if (motorState1)
        {
           if(UltrasonicModule::getLatestDistanceMm0() < 100)
           {
             LOG_PRINTLN(LOG_ULTRASONIC, "[Ultrasonic] 检测到左前方障碍物, 开始震动");
             setMotors(1, 1, 0, 0);
           }
           if(UltrasonicModule::getLatestDistanceMm1() < 100)
           {
            LOG_PRINTLN(LOG_ULTRASONIC, "[Ultrasonic] 检测到右前方障碍物, 开始震动");
             setMotors(1, 0, 0, 1);
           }
        }
        else
        {
          stopAllMotors();
          LOG_PRINTLN(LOG_MOTOR, "[Motor] 马达关闭");
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

bool enqueueMotorCmd(MotorCmdType cmd, int angle)
{
  if (motorCmdQueue == NULL)
  {
    return false;
  }

  MotorCommand motorCmd;
  motorCmd.cmd = cmd;
  motorCmd.angle = angle;
  return xQueueSend(motorCmdQueue, &motorCmd, 0) == pdPASS;
}

static bool initImpl()
{
  randomSeed((uint32_t)esp_random());

  LOG_PRINTLN(LOG_MOTOR, "[Setup] 初始化振动马达引脚...");
  initMotorPins();

  LOG_PRINTLN(LOG_MOTOR, "[Setup] 创建Motor队列...");
  motorCmdQueue = xQueueCreate(5, sizeof(MotorCommand));
  if (motorCmdQueue == NULL)
  {
    LOG_PRINTLN(LOG_MOTOR, "[Setup] 错误: Motor队列创建失败!");
    return false;
  }

  s_motorInitialized = true;
  return true;
}

static bool startTasksImpl()
{
  if (!s_motorInitialized)
  {
    LOG_PRINTLN(LOG_MOTOR, "[Setup] 错误: motor 模块尚未初始化");
    return false;
  }

  BaseType_t ok =
      xTaskCreatePinnedToCore(motorTask, "Motor_Task", 4096, NULL, 1, NULL, 0);
  if (ok != pdPASS)
  {
    LOG_PRINTLN(LOG_MOTOR, "[Setup] 错误: Motor任务创建失败");
    return false;
  }

  LOG_PRINTLN(LOG_MOTOR, "[Setup] Motor模块启动完成!");
  return true;
}
}  // namespace esp32s3

bool esp32s3::MotorModule::init() { return esp32s3::initImpl(); }
bool esp32s3::MotorModule::startTasks() { return esp32s3::startTasksImpl(); }

bool esp32s3::MotorModule::startByAngle(int angle)
{
  return esp32s3::enqueueMotorCmd(esp32s3::MOTOR_CMD_START, angle);
}

bool esp32s3::MotorModule::stop()
{
  return esp32s3::enqueueMotorCmd(esp32s3::MOTOR_CMD_STOP, 0);
}
