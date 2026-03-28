#include "ultrasonic/ultrasonic.h"

#include "LogSwitch.h"

namespace ultrasonic
{
static uint8_t s_trigPin = 27;
static uint8_t s_echoPin = 14;
static volatile float s_latestDistanceMm = -1.0f;
static bool s_initialized = false;
static const uint32_t ULTRASONIC_TASK_STACK = 4096;

static const uint32_t ECHO_TIMEOUT_US = 12000;  // ~2m, 减少阻塞避免看门狗压力
static const uint32_t SAMPLE_INTERVAL_MS = 100;

void begin(uint8_t trigPin, uint8_t echoPin)
{
  s_trigPin = trigPin;
  s_echoPin = echoPin;

  pinMode(s_trigPin, OUTPUT);
  pinMode(s_echoPin, INPUT);
  digitalWrite(s_trigPin, LOW);
  Serial.println("[Ultrasonic] 初始化完成, trigPin=" + String(s_trigPin) +
                 ", echoPin=" + String(s_echoPin));
  LOG_PRINTF(LOG_ULTRASONIC, "[Ultrasonic] init done, trig=%u echo=%u\n",
             s_trigPin, s_echoPin);
  s_initialized = true;
}

bool initUltrasonicModule(uint8_t trigPin, uint8_t echoPin)
{
  begin(trigPin, echoPin);
  return true;
}

float measureDistanceMm()
{
  digitalWrite(s_trigPin, LOW);
  delayMicroseconds(5);
  digitalWrite(s_trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(s_trigPin, LOW);

  unsigned long durationUs = pulseIn(s_echoPin, HIGH, ECHO_TIMEOUT_US);
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

float getLatestDistanceMm() { return s_latestDistanceMm; }

void task(void* pvParameters)
{
  LOG_PRINTLN(LOG_ULTRASONIC, "[Ultrasonic] task started");

  while (true)
  {
    float distanceMm = measureDistanceMm();
    s_latestDistanceMm = distanceMm;

    if (distanceMm < 0)
    {
      LOG_PRINTLN(LOG_ULTRASONIC, "[Ultrasonic] timeout");
    }
    else
    {
      LOG_PRINTF(LOG_ULTRASONIC, "[Ultrasonic] distance: %.1f mm\n",
                 distanceMm);
    }

    vTaskDelay(pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));
  }
}

bool startUltrasonicTask()
{
  if (!s_initialized)
  {
    LOG_PRINTLN(LOG_ULTRASONIC, "[Ultrasonic] init required before start task");
    return false;
  }

  BaseType_t ok = xTaskCreatePinnedToCore(
      task, "UltrasonicTask", ULTRASONIC_TASK_STACK, NULL, 1, NULL, 0);
  return ok == pdPASS;
}
}  // namespace ultrasonic