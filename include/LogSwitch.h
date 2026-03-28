#ifndef LOG_SWITCH_H
#define LOG_SWITCH_H

// 通过 build_flags 覆盖这些宏，例如: -D LOG_BLE=0
#ifndef LOG_BATTERY
#define LOG_BATTERY 0
#endif

#ifndef LOG_BLE
#define LOG_BLE 1
#endif

#ifndef LOG_MPU
#define LOG_MPU 1
#endif

#ifndef LOG_CAMERA
#define LOG_CAMERA 1
#endif

#ifndef LOG_WIFI
#define LOG_WIFI 1
#endif

#ifndef LOG_MOTOR
#define LOG_MOTOR 0
#endif

#ifndef LOG_ULTRASONIC
#define LOG_ULTRASONIC 1
#endif

#define LOG_PRINT(enabled, msg)     \
  do                                \
  {                                 \
    if (enabled) Serial.print(msg); \
  } while (0)

#define LOG_PRINTLN(enabled, msg)     \
  do                                  \
  {                                   \
    if (enabled) Serial.println(msg); \
  } while (0)

#define LOG_PRINTF(enabled, ...)             \
  do                                         \
  {                                          \
    if (enabled) Serial.printf(__VA_ARGS__); \
  } while (0)

#define LOG_ESP_I(enabled, tag, fmt, ...)           \
  do                                                \
  {                                                 \
    if (enabled) ESP_LOGI(tag, fmt, ##__VA_ARGS__); \
  } while (0)

#define LOG_ESP_W(enabled, tag, fmt, ...)           \
  do                                                \
  {                                                 \
    if (enabled) ESP_LOGW(tag, fmt, ##__VA_ARGS__); \
  } while (0)

#define LOG_ESP_E(enabled, tag, fmt, ...)           \
  do                                                \
  {                                                 \
    if (enabled) ESP_LOGE(tag, fmt, ##__VA_ARGS__); \
  } while (0)

#endif
