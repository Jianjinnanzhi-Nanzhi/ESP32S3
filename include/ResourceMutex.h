#ifndef RESOURCE_MUTEX_H
#define RESOURCE_MUTEX_H

#include <Arduino.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"


class ResourceMutex
{
 public:
  ResourceMutex() : mutex_(NULL) {}

  bool begin()
  {
    if (mutex_ == NULL)
    {
      mutex_ = xSemaphoreCreateMutex();
    }
    return mutex_ != NULL;
  }

  bool lock(uint32_t timeoutMs)
  {
    if (mutex_ == NULL)
    {
      return false;
    }
    return xSemaphoreTake(mutex_, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
  }

  void unlock()
  {
    if (mutex_ != NULL)
    {
      xSemaphoreGive(mutex_);
    }
  }

 private:
  SemaphoreHandle_t mutex_;
};

#endif