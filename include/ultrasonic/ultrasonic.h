#ifndef ULTRASONIC_SERVICE_H
#define ULTRASONIC_SERVICE_H

#include <Arduino.h>

#ifndef ULTRASONIC_TRIG_PIN1
#define ULTRASONIC_TRIG_PIN1 47
#endif

#ifndef ULTRASONIC_ECHO_PIN1
#define ULTRASONIC_ECHO_PIN1 48
#endif

#ifndef ULTRASONIC_TRIG_PIN0
#define ULTRASONIC_TRIG_PIN0 37
#endif

#ifndef ULTRASONIC_ECHO_PIN0
#define ULTRASONIC_ECHO_PIN0 38
#endif

namespace esp32s3
{
  class UltrasonicModule
  {
  public:
    static bool init0(uint8_t trigPin = ULTRASONIC_TRIG_PIN0,
                            uint8_t echoPin = ULTRASONIC_ECHO_PIN0);
    static bool init1(uint8_t trigPin = ULTRASONIC_TRIG_PIN1,
                            uint8_t echoPin = ULTRASONIC_ECHO_PIN1);
                            
    static bool startTask();
    static void begin0(uint8_t trigPin = ULTRASONIC_TRIG_PIN0,
            uint8_t echoPin = ULTRASONIC_ECHO_PIN0);
    static float measureDistanceMm0();
    static float getLatestDistanceMm0();

    static void begin1(uint8_t trigPin = ULTRASONIC_TRIG_PIN1,
            uint8_t echoPin = ULTRASONIC_ECHO_PIN1);
    static float measureDistanceMm1();
    static float getLatestDistanceMm1();

    static void task(void* pvParameters);
  };
}  // namespace esp32s3

#endif
