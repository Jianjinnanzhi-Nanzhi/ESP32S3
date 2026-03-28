#ifndef ULTRASONIC_SERVICE_H
#define ULTRASONIC_SERVICE_H

#include <Arduino.h>

#ifndef ULTRASONIC_TRIG_PIN
#define ULTRASONIC_TRIG_PIN 35
#endif

#ifndef ULTRASONIC_ECHO_PIN
#define ULTRASONIC_ECHO_PIN 36
#endif

namespace ultrasonic
{
bool initUltrasonicModule(uint8_t trigPin = ULTRASONIC_TRIG_PIN,
                          uint8_t echoPin = ULTRASONIC_ECHO_PIN);
bool startUltrasonicTask();
void begin(uint8_t trigPin = ULTRASONIC_TRIG_PIN,
           uint8_t echoPin = ULTRASONIC_ECHO_PIN);
float measureDistanceMm();
float getLatestDistanceMm();
void task(void* pvParameters);
}  // namespace ultrasonic

#endif