#include "motor/MotorBleApp.h"

#include <Arduino.h>
#include <WiFi.h>

#include "LogSwitch.h"
#include "motor/ble_mpu.h"
#include "motor/mpu_sensor.h"

namespace motor_app
{
// ========== 硬件引脚定义 ==========
#define LED_PIN 2
#define FALL_THRESHOLD 3.0

// ========== 振动马达引脚定义 ==========
#define MOTOR_PIN_1 39
#define MOTOR_PIN_2 40
#define MOTOR_PIN_3 41
#define MOTOR_PIN_4 42

MPU_Sensor mpu;
BLE_MPU ble;

QueueHandle_t wifiConfigQueue = NULL;
bool wifiConnected = false;

QueueHandle_t motorCmdQueue = NULL;
bool motorRunning = false;
int motorTargetAngle = 0;

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

TaskHandle_t mpuTaskHandle = NULL;
TaskHandle_t bleTaskHandle = NULL;
TaskHandle_t batteryTaskHandle = NULL;
TaskHandle_t wifiTaskHandle = NULL;

QueueHandle_t mpuDataQueue = NULL;
QueueHandle_t bleMessageQueue = NULL;
bool s_motorInitialized = false;

typedef struct
{
  char ssid[64];
  char password[64];
} WiFiConfig;

typedef struct
{
  float ax, ay, az;
  float gx, gy, gz;
  bool fallDetected;
} SensorData;

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

void bleDataCallback(String data)
{
  LOG_PRINTLN(LOG_BLE, "[BLE] ========== BLE数据接收回调 ==========");
  LOG_PRINT(LOG_BLE, "[BLE] 收到原始数据: ");
  LOG_PRINTLN(LOG_BLE, data);

  if (data.equals("GET_DATA"))
  {
    LOG_PRINTLN(LOG_BLE, "[BLE] 命令: 请求传感器数据");
    xQueueSend(bleMessageQueue, "SEND_DATA", 0);
  }
  else if (data.startsWith("SET_THRESHOLD:"))
  {
    LOG_PRINTLN(LOG_BLE, "[BLE] 命令: 设置跌倒阈值");
    String valStr = data.substring(14);
    float newThreshold = valStr.toFloat();
    LOG_PRINTF(LOG_BLE, "[BLE] 新阈值: %.2f g²\n", newThreshold);
  }
  else if (data.startsWith("WIFI:"))
  {
    LOG_PRINTLN(LOG_BLE, "[BLE] 命令: WiFi配网");
    int firstColon = data.indexOf(':', 5);
    if (firstColon > 0)
    {
      String ssid = data.substring(5, firstColon);
      String password = data.substring(firstColon + 1);

      LOG_PRINT(LOG_BLE, "[BLE] WiFi SSID: ");
      LOG_PRINTLN(LOG_BLE, ssid);
      LOG_PRINT(LOG_BLE, "[BLE] WiFi Password: ");
      LOG_PRINTLN(LOG_BLE, password);

      WiFiConfig wifiConfig;
      ssid.toCharArray(wifiConfig.ssid, sizeof(wifiConfig.ssid));
      password.toCharArray(wifiConfig.password, sizeof(wifiConfig.password));
      xQueueSend(wifiConfigQueue, &wifiConfig, 0);
    }
    else
    {
      LOG_PRINTLN(LOG_BLE,
                  "[BLE] 错误: WiFi配网格式不正确,应为 WIFI:账号:密码");
    }
  }
  else if (data.startsWith("change:"))
  {
    LOG_PRINTLN(LOG_BLE, "[BLE] 命令: 改变振动");
    String angleStr = data.substring(7);
    int angle = angleStr.toInt();

    if (angle < -180) angle = -180;
    if (angle > 180) angle = 180;

    LOG_PRINTF(LOG_BLE, "[BLE] 目标角度: %d\n", angle);

    MotorCommand cmd;
    cmd.cmd = MOTOR_CMD_START;
    cmd.angle = angle;
    xQueueSend(motorCmdQueue, &cmd, 0);
  }
  else if (data.equals("end"))
  {
    LOG_PRINTLN(LOG_BLE, "[BLE] 命令: 结束振动");

    MotorCommand cmd;
    cmd.cmd = MOTOR_CMD_STOP;
    cmd.angle = 0;
    xQueueSend(motorCmdQueue, &cmd, 0);
  }
  else
  {
    LOG_PRINTLN(LOG_BLE, "[BLE] 未知命令");
  }
  LOG_PRINTLN(LOG_BLE, "[BLE] ========================================");
}

void mpuTask(void* pvParameters)
{
  SensorData sensorData;
  bool fallFlag = false;
  unsigned long fallCooldown = 0;

  LOG_PRINTLN(LOG_MPU, "[MPU Task] 任务已启动, 运行在 Core 1");

  while (true)
  {
    mpu.update();
    mpu.getMotion6(&sensorData.ax, &sensorData.ay, &sensorData.az,
                   &sensorData.gx, &sensorData.gy, &sensorData.gz);

    bool isFalling = mpu.checkFall(FALL_THRESHOLD);
    unsigned long now = millis();

    if (isFalling && !fallFlag && (now - fallCooldown > 3000))
    {
      sensorData.fallDetected = true;
      fallFlag = true;
      fallCooldown = now;
      digitalWrite(LED_PIN, HIGH);

      LOG_PRINTLN(LOG_MPU, "[MPU] !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
      LOG_PRINTLN(LOG_MPU, "[MPU] !!!!!!!! 跌倒检测触发 !!!!!!!!!!");
      LOG_PRINTF(LOG_MPU, "[MPU] !!!! 时间戳: %lu ms\n", now);
      LOG_PRINTLN(LOG_MPU, "[MPU] !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    }
    else
    {
      sensorData.fallDetected = false;
      if (fallFlag && (now - fallCooldown > 500))
      {
        fallFlag = false;
        digitalWrite(LED_PIN, LOW);
        LOG_PRINTLN(LOG_MPU, "[MPU] 跌倒状态复位, LED熄灭");
      }
    }

    BaseType_t qResult = xQueueSend(mpuDataQueue, &sensorData, 0);
    if (qResult != pdPASS)
    {
      LOG_PRINTLN(LOG_MPU, "[MPU] 警告: 队列已满,数据丢弃");
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void bleTask(void* pvParameters)
{
  SensorData sensorData;
  char cmdBuffer[32];

  LOG_PRINTLN(LOG_BLE, "[BLE Task] 任务已启动, 运行在 Core 0");

  while (true)
  {
    if (xQueueReceive(mpuDataQueue, &sensorData, pdMS_TO_TICKS(100)) == pdPASS)
    {
      if (sensorData.fallDetected)
      {
        LOG_PRINTLN(LOG_BLE, "[BLE Task] 收到跌倒事件,发送BLE告警");
        ble.sendFallAlert();
      }
    }

    if (xQueueReceive(bleMessageQueue, cmdBuffer, 0) == pdPASS)
    {
      LOG_PRINT(LOG_BLE, "[BLE Task] 收到命令: ");
      LOG_PRINTLN(LOG_BLE, cmdBuffer);

      if (String(cmdBuffer) == "SEND_DATA")
      {
        float ax, ay, az, gx, gy, gz;
        mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
        LOG_PRINTLN(LOG_BLE, "[BLE Task] 发送传感器数据到BLE");
        ble.sendSensorData(ax, ay, az, gx, gy, gz);
      }
    }

    ble.tick();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void batteryTask(void* pvParameters)
{
  int lastBattery = -1;

  LOG_PRINTLN(LOG_BATTERY, "[Battery Task] 任务已启动, 运行在 Core 0");

  while (true)
  {
    int battery = random(1, 101);

    if (battery != lastBattery)
    {
      LOG_PRINTF(LOG_BATTERY, "[Battery Task] 电量变化: %d%%\n", battery);
      ble.sendBattery(battery);
      lastBattery = battery;
    }
    else
    {
      LOG_PRINTF(LOG_BATTERY, "[Battery Task] 电量未变化: %d%%\n", battery);
    }

    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

void wifiTask(void* pvParameters)
{
  WiFiConfig wifiConfig;
  MotorCommand motorCmd;

  bool motorState = false;
  unsigned long motorLastToggle = 0;
  bool currentM1 = false;
  bool currentM2 = false;
  bool currentM3 = false;
  bool currentM4 = false;

  LOG_PRINTLN(LOG_WIFI, "[WiFi+Motor Task] 任务已启动, 运行在 Core 0");

  while (true)
  {
    if (xQueueReceive(wifiConfigQueue, &wifiConfig, 0) == pdPASS)
    {
      LOG_PRINTLN(LOG_WIFI, "[WiFi Task] 收到WiFi配置,开始连接...");
      LOG_PRINTF(LOG_WIFI, "[WiFi Task] SSID: %s\n", wifiConfig.ssid);
      LOG_PRINTF(LOG_WIFI, "[WiFi Task] Password: %s\n", wifiConfig.password);

      WiFi.begin(wifiConfig.ssid, wifiConfig.password);

      LOG_PRINTLN(LOG_WIFI, "[WiFi Task] 正在连接WiFi...");
      int attempts = 0;
      while (WiFi.status() != WL_CONNECTED && attempts < 30)
      {
        vTaskDelay(pdMS_TO_TICKS(500));
        LOG_PRINT(LOG_WIFI, ".");
        attempts++;
      }
      LOG_PRINTLN(LOG_WIFI, "");

      if (WiFi.status() == WL_CONNECTED)
      {
        wifiConnected = true;
        LOG_PRINTLN(LOG_WIFI,
                    "[WiFi Task] !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        LOG_PRINTLN(LOG_WIFI, "[WiFi Task] !!!!! WiFi连接成功 !!!!!!!!!!");
        LOG_PRINT(LOG_WIFI, "[WiFi Task] IP地址: ");
        LOG_PRINTLN(LOG_WIFI, WiFi.localIP());
        LOG_PRINTLN(LOG_WIFI,
                    "[WiFi Task] !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        ble.sendMessage("WIFI_CONNECTED:" + WiFi.localIP().toString());
      }
      else
      {
        wifiConnected = false;
        LOG_PRINTLN(LOG_WIFI, "[WiFi Task] WiFi连接失败!");
        ble.sendMessage("WIFI_FAILED");
      }
    }

    if (wifiConnected)
    {
      if (WiFi.status() != WL_CONNECTED)
      {
        wifiConnected = false;
        LOG_PRINTLN(LOG_WIFI, "[WiFi Task] WiFi已断开连接");
        ble.sendMessage("WIFI_DISCONNECTED");
      }
    }

    if (xQueueReceive(motorCmdQueue, &motorCmd, 0) == pdPASS)
    {
      if (motorCmd.cmd == MOTOR_CMD_START)
      {
        motorRunning = true;
        motorTargetAngle = motorCmd.angle;
        motorState = true;
        motorLastToggle = millis();

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
      if (now - motorLastToggle >= 1000)
      {
        motorState = !motorState;
        motorLastToggle = now;

        if (motorState)
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
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

bool initImpl()
{
  randomSeed((uint32_t)esp_random());

  LOG_PRINTLN(LOG_BLE, "\n\n");
  LOG_PRINTLN(LOG_BLE, "==========================================");
  LOG_PRINTLN(LOG_BLE, "  ESP32-S3 MPU6050 + BLE + FreeRTOS");
  LOG_PRINTLN(LOG_BLE, "  低功耗蓝牙版本");
  LOG_PRINTLN(LOG_BLE, "==========================================");

  LOG_PRINTLN(LOG_BLE, "[Setup] 初始化LED引脚...");
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  LOG_PRINTLN(LOG_MOTOR, "[Setup] 初始化振动马达引脚...");
  initMotorPins();

  LOG_PRINTLN(LOG_BLE, "[Setup] 创建FreeRTOS队列...");
  mpuDataQueue = xQueueCreate(10, sizeof(SensorData));
  bleMessageQueue = xQueueCreate(5, 32);
  wifiConfigQueue = xQueueCreate(2, sizeof(WiFiConfig));
  motorCmdQueue = xQueueCreate(5, sizeof(MotorCommand));

  if (mpuDataQueue == NULL || bleMessageQueue == NULL ||
      wifiConfigQueue == NULL || motorCmdQueue == NULL)
  {
    LOG_PRINTLN(LOG_BLE, "[Setup] 错误: 队列创建失败!");
    return false;
  }

  LOG_PRINTLN(LOG_MPU, "[Setup] 初始化MPU6050传感器...");
  if (!mpu.init())
  {
    LOG_PRINTLN(LOG_MPU, "[Setup] 错误: MPU6050初始化失败!");
    LOG_PRINTLN(LOG_MPU, "[Setup] 请检查I2C连接 (SDA=GPIO21, SCL=GPIO20)");
    LOG_PRINTLN(LOG_MPU,
                "[Setup] 进入降级模式: 跳过MPU相关能力,继续启动其他模块");
  }
  else
  {
    LOG_PRINTLN(LOG_MPU, "[Setup] MPU6050初始化成功!");
  }

  LOG_PRINTLN(LOG_BLE, "[Setup] 初始化低功耗蓝牙...");
  ble.init("ESP32_MPU6050");
  ble.setCallback(bleDataCallback);
  LOG_PRINTLN(LOG_BLE, "[Setup] BLE初始化完成! 设备名: ESP32_MPU6050");

  s_motorInitialized = true;
  return true;
}

bool startTasksImpl()
{
  if (!s_motorInitialized)
  {
    LOG_PRINTLN(LOG_BLE, "[Setup] 错误: motor 模块尚未初始化");
    return false;
  }

  LOG_PRINTLN(LOG_BLE, "[Setup] 创建FreeRTOS任务...");

  xTaskCreatePinnedToCore(bleTask, "BLE_Task", 4096, NULL, 1, &bleTaskHandle,
                          0);
  xTaskCreatePinnedToCore(batteryTask, "Battery_Task", 2048, NULL, 1,
                          &batteryTaskHandle, 0);
  xTaskCreatePinnedToCore(wifiTask, "WiFi+Motor_Task", 4096, NULL, 1,
                          &wifiTaskHandle, 0);

  LOG_PRINTLN(LOG_BLE, "[Setup] 系统初始化完成!");
  LOG_PRINTLN(LOG_BLE, "==========================================\n");
  return true;
}
}  // namespace motor_app

bool initMotorBleApp() { return motor_app::initImpl(); }
bool startMotorBleTasks() { return motor_app::startTasksImpl(); }

void startMotorBleApp()
{
  if (!initMotorBleApp())
  {
    return;
  }
  (void)startMotorBleTasks();
}
