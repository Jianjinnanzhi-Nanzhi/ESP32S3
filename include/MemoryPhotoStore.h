#ifndef MEMORY_PHOTO_STORE_H
#define MEMORY_PHOTO_STORE_H

#include <Arduino.h>

#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

struct MemoryPhotoMeta
{
  uint32_t id;
  uint64_t tsMs;
  size_t len;
};

class MemoryPhotoStore
{
 public:
  explicit MemoryPhotoStore(size_t capacity);
  ~MemoryPhotoStore();

  bool begin();
  bool pushOwnedFrame(uint8_t* buf, size_t len, uint64_t tsMs);
  bool getRecent(std::vector<MemoryPhotoMeta>& out, size_t maxCount = 0);
  bool cloneFrameById(uint32_t id, uint8_t** outBuf, size_t* outLen);
  bool deleteById(uint32_t id);
  size_t clearAll();
  size_t size();
  size_t capacity() const { return slots_.size(); }

 private:
  struct Slot
  {
    uint32_t id;
    uint64_t tsMs;
    size_t len;
    uint8_t* data;
    bool valid;
  };

  bool lock(uint32_t timeoutMs);
  void unlock();

  std::vector<Slot> slots_;
  SemaphoreHandle_t mutex_;
  size_t writeIndex_;
  size_t count_;
  uint32_t nextId_;
};

#endif