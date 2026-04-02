#include "ble/BleApp.h"

#include <Arduino.h>

#include "LogSwitch.h"
#include "motor/MotorBleApp.h"
#include "motor/ble_mpu.h"
#include "motor/mpu_sensor.h"

namespace ble
{
#define LED_PIN 2
#define FALL_THRESHOLD 5.0
#define MPU_TASK_PERIOD_MS 10
#define BATTERY_TASK_STACK_WORDS 4096

BLE_MPU ble;
MPU_Sensor mpu;

TaskHandle_t mpuTaskHandle = NULL;
TaskHandle_t bleTaskHandle = NULL;
TaskHandle_t batteryTaskHandle = NULL;

QueueHandle_t mpuDataQueue = NULL;
QueueHandle_t bleMessageQueue = NULL;

bool s_initialized = false;
bool s_mpuReady = false;
volatile float s_fallThresholdG2 = FALL_THRESHOLD;
BleExternalCommandHandler s_externalHandler = nullptr;

typedef struct
{
  float ax, ay, az;
  float gx, gy, gz;
  bool fallDetected;
} SensorData;

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
    if (newThreshold >= 0.5f && newThreshold <= 20.0f)
    {
      s_fallThresholdG2 = newThreshold;
      LOG_PRINTF(LOG_BLE, "[BLE] 新阈值已生效: %.2f g²\n", s_fallThresholdG2);
    }
    else
    {
      LOG_PRINTF(LOG_BLE, "[BLE] 阈值无效(%.2f), 允许范围: 0.5~20.0 g²\n",
                 newThreshold);
    }
  }
  else if (data.startsWith("change:"))
  {
    LOG_PRINTLN(LOG_BLE, "[BLE] 命令: 改变振动");
    String angleStr = data.substring(7);
    int angle = angleStr.toInt();

    LOG_PRINTF(LOG_BLE, "[BLE] 目标角度: %d\n", angle);
    (void)motorStartByAngle(angle);
  }
  else if (data.equals("end"))
  {
    LOG_PRINTLN(LOG_BLE, "[BLE] 命令: 结束振动");
    (void)motorStop();
  }
  else if (s_externalHandler != nullptr && s_externalHandler(data))
  {
    LOG_PRINTLN(LOG_BLE, "[BLE] 命令: 外部模块已处理");
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

    bool isFalling = mpu.checkFall(s_fallThresholdG2);
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

    vTaskDelay(pdMS_TO_TICKS(MPU_TASK_PERIOD_MS));
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

  int battery = random(81, 101);

  while (true)
  {
    UBaseType_t stackWordsLeft = uxTaskGetStackHighWaterMark(NULL);
    if (stackWordsLeft < 512)
    {
      LOG_PRINTF(LOG_BATTERY,
                 "[Battery Task] 警告: 剩余栈仅 %u words, 可能接近溢出\n",
                 (unsigned)stackWordsLeft);
    }

    if (battery != lastBattery)
    {
      LOG_PRINTF(LOG_BATTERY, "[Battery Task] 电量变化: %d%%\n", battery);
      ble.sendBattery(battery);
      lastBattery = battery;
    }
    else
    {
      int time = random(1, 5);
      vTaskDelay(pdMS_TO_TICKS(time * 100 * 60));
    }
  }
}

bool initImpl()
{
  randomSeed((uint32_t)esp_random());

  LOG_PRINTLN(LOG_BLE, "[Setup] 初始化BLE状态LED引脚...");
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  LOG_PRINTLN(LOG_BLE, "[Setup] 创建BLE/MPU队列...");
  mpuDataQueue = xQueueCreate(10, sizeof(SensorData));
  bleMessageQueue = xQueueCreate(5, 32);

  if (mpuDataQueue == NULL || bleMessageQueue == NULL)
  {
    LOG_PRINTLN(LOG_BLE, "[Setup] 错误: BLE/MPU队列创建失败!");
    return false;
  }

  LOG_PRINTLN(LOG_MPU, "[Setup] 初始化MPU6050传感器...");
  if (!mpu.init())
  {
    LOG_PRINTLN(LOG_MPU, "[Setup] 错误: MPU6050初始化失败!");
    LOG_PRINTLN(LOG_MPU,
                "[Setup] 进入降级模式: 跳过MPU相关能力,继续启动其他模块");
    s_mpuReady = false;
  }
  else
  {
    LOG_PRINTLN(LOG_MPU, "[Setup] MPU6050初始化成功!");
    s_mpuReady = true;
  }

  LOG_PRINTLN(LOG_BLE, "[Setup] 初始化低功耗蓝牙...");
  ble.init("ESP32_MPU6050");
  ble.setCallback(bleDataCallback);
  LOG_PRINTLN(LOG_BLE, "[Setup] BLE初始化完成! 设备名: ESP32_MPU6050");

  s_initialized = true;
  return true;
}

bool startTasksImpl()
{
  if (!s_initialized)
  {
    LOG_PRINTLN(LOG_BLE, "[Setup] 错误: BLE模块尚未初始化");
    return false;
  }

  LOG_PRINTLN(LOG_BLE, "[Setup] 创建BLE/MPU任务...");

  if (s_mpuReady)
  {
    xTaskCreatePinnedToCore(mpuTask, "MPU_Task", 4096, NULL, 2, &mpuTaskHandle,
                            1);
    LOG_PRINTLN(LOG_MPU, "[Setup] MPU任务已创建(Core 1)");
  }
  else
  {
    LOG_PRINTLN(LOG_MPU, "[Setup] MPU未就绪, 跳过MPU任务创建");
  }

  xTaskCreatePinnedToCore(bleTask, "BLE_Task", 4096, NULL, 1, &bleTaskHandle,
                          0);
  xTaskCreatePinnedToCore(batteryTask, "Battery_Task", BATTERY_TASK_STACK_WORDS,
                          NULL, 1, &batteryTaskHandle, 0);

  LOG_PRINTLN(LOG_BLE, "[Setup] BLE模块启动完成!");
  return true;
}
}  // namespace ble

bool initBleApp() { return ble::initImpl(); }
bool startBleTasks() { return ble::startTasksImpl(); }

void bleSendMessage(const String& message) { ble::ble.sendMessage(message); }

void bleRegisterExternalCommandHandler(BleExternalCommandHandler handler)
{
  ble::s_externalHandler = handler;
}
