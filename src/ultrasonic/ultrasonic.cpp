#include "ultrasonic/ultrasonic.h"

#include "LogSwitch.h"

namespace esp32s3
{
static uint8_t s_trigPin0= 37;
static uint8_t s_echoPin0 = 38;

static uint8_t s_trigPin1= 35;
static uint8_t s_echoPin1 =36;

static volatile float s_latestDistanceMm0 = -1.0f;
static volatile float s_latestDistanceMm1 = -1.0f;

static bool s_initialized = false;
static const uint32_t ULTRASONIC_TASK_STACK = 4096;

static const uint32_t ECHO_TIMEOUT_US = 12000;  // ~2m, 减少阻塞避免看门狗压力
static const uint32_t SAMPLE_INTERVAL_MS = 100;

static bool isReservedBySpiMemory(uint8_t pin)
{
  // ESP32-S3 的 GPIO26~GPIO37 连接 SPI0/1，常用于外部 Flash/PSRAM。
  // 这些引脚被重配为普通 GPIO 时，可能直接破坏堆并触发 LoadProhibited。
  return pin >= 26 && pin <= 37;
}

void UltrasonicModule::begin0(uint8_t trigPin, uint8_t echoPin)
{
  s_trigPin0 = trigPin;
  s_echoPin0 = echoPin;

  pinMode(s_trigPin0, OUTPUT);
  pinMode(s_echoPin0, INPUT);
  digitalWrite(s_trigPin0, LOW);
  Serial.println("[Ultrasonic] 初始化完成, trigPin=" + String(s_trigPin0) +
                 ", echoPin=" + String(s_echoPin0));
  LOG_PRINTF(LOG_ULTRASONIC, "[Ultrasonic] init done, trig=%u echo=%u\n",
             s_trigPin0, s_echoPin0);
  s_initialized = true;
}

void UltrasonicModule::begin1(uint8_t trigPin, uint8_t echoPin)
{
  s_trigPin1 = trigPin;
  s_echoPin1 = echoPin;

  pinMode(s_trigPin1, OUTPUT);
  pinMode(s_echoPin1, INPUT);
  digitalWrite(s_trigPin1, LOW);
  Serial.println("[Ultrasonic] 初始化完成, trigPin=" + String(s_trigPin1) +
                 ", echoPin=" + String(s_echoPin1));
  LOG_PRINTF(LOG_ULTRASONIC, "[Ultrasonic] init done, trig=%u echo=%u\n",
             s_trigPin1, s_echoPin1);
  s_initialized = true;
}

bool UltrasonicModule::init0(uint8_t trigPin, uint8_t echoPin)
{
  /*if (isReservedBySpiMemory(trigPin) || isReservedBySpiMemory(echoPin))
  {
    LOG_PRINTF(LOG_ULTRASONIC,
               "[Ultrasonic] invalid pins: trig=%u echo=%u; "
               "GPIO26~37 are reserved for SPI flash/PSRAM on ESP32-S3\n",
               trigPin, echoPin);
    return false;
  }
  */

  if (trigPin == echoPin)
  {
    LOG_PRINTF(LOG_ULTRASONIC,
               "[Ultrasonic] invalid pins: trig and echo must differ (%u)\n",
               trigPin);
    return false;
  }
  begin0(trigPin, echoPin);
  return true;
}

bool UltrasonicModule::init1(uint8_t trigPin, uint8_t echoPin)
{
 /*
 if (isReservedBySpiMemory(trigPin) || isReservedBySpiMemory(echoPin))
  {
    LOG_PRINTF(LOG_ULTRASONIC,
               "[Ultrasonic] invalid pins: trig=%u echo=%u; "
               "GPIO26~37 are reserved for SPI flash/PSRAM on ESP32-S3\n",
               trigPin, echoPin);
    return false;
  }
  */

  if (trigPin == echoPin)
  {
    LOG_PRINTF(LOG_ULTRASONIC,
               "[Ultrasonic] invalid pins: trig and echo must differ (%u)\n",
               trigPin);
    return false;
  }
  begin1(trigPin, echoPin);
  return true;
}

float UltrasonicModule::measureDistanceMm0()
{
  digitalWrite(s_trigPin0, LOW);
  delayMicroseconds(5);
  digitalWrite(s_trigPin0, HIGH);
  delayMicroseconds(10);
  digitalWrite(s_trigPin0, LOW);

  unsigned long durationUs = pulseIn(s_echoPin0, HIGH, ECHO_TIMEOUT_US);
  if (durationUs == 0)
  {
    return -1.0f;
  }

  float distanceMm = (durationUs * 0.3432f) / 2.0f;
  if (distanceMm < 30.0f)
  {
    distanceMm = 0.0f;
  }
  else if (distanceMm > 3800.0f)
  {
    distanceMm = 3800.0f;
  }
  return distanceMm;
}

float UltrasonicModule::measureDistanceMm1()
{
  digitalWrite(s_trigPin1, LOW);
  delayMicroseconds(5);
  digitalWrite(s_trigPin1, HIGH);
  delayMicroseconds(10);
  digitalWrite(s_trigPin1, LOW);

  unsigned long durationUs = pulseIn(s_echoPin1, HIGH, ECHO_TIMEOUT_US);
  if (durationUs == 0)
  {
    return -1.0f;
  }

  float distanceMm = (durationUs * 0.3432f) / 2.0f;
  if (distanceMm < 30.0f)
  {
    distanceMm = 0.0f;
  }
  else if (distanceMm > 3800.0f)
  {
    distanceMm = 3800.0f;
  }
  return distanceMm;
}

float UltrasonicModule::getLatestDistanceMm0() { return s_latestDistanceMm0; }
float UltrasonicModule::getLatestDistanceMm1() { return s_latestDistanceMm1; }

void UltrasonicModule::task(void* pvParameters)
{
  LOG_PRINTLN(LOG_ULTRASONIC, "[Ultrasonic] task started");

  while (true)
  {
    float distanceMm0 = measureDistanceMm0();
    s_latestDistanceMm0 = distanceMm0;
    float distanceMm1 = measureDistanceMm1();
    s_latestDistanceMm1 = distanceMm1;

    if (distanceMm0 < 0)
    {
      LOG_PRINTLN(LOG_ULTRASONIC, "[Ultrasonic] timeout0");
    }
    else
    {
      LOG_PRINTF(LOG_ULTRASONIC, "[Ultrasonic] distance0: %.1f mm\n",
                 distanceMm0);
    }
    if (distanceMm1 < 0)
    {
      LOG_PRINTLN(LOG_ULTRASONIC, "[Ultrasonic] timeout1");
    }
    else
    {
      LOG_PRINTF(LOG_ULTRASONIC, "[Ultrasonic] distance1: %.1f mm\n",
                 distanceMm1);
    }

    vTaskDelay(pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));
  }
}

bool UltrasonicModule::startTask()
{
  if (!s_initialized)
  {
    LOG_PRINTLN(LOG_ULTRASONIC, "[Ultrasonic] init required before start task");
    return false;
  }

  BaseType_t ok = xTaskCreatePinnedToCore(
      UltrasonicModule::task, "UltrasonicTask", ULTRASONIC_TASK_STACK, NULL, 1, NULL, 0);
  return ok == pdPASS;
}
}  // namespace esp32s3
