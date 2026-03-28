#include "camera/MemoryPhotoStore.h"

#include <string.h>

#include <algorithm>

MemoryPhotoStore::MemoryPhotoStore(size_t capacity)
    : mutex_(NULL), writeIndex_(0), count_(0), nextId_(1)
{
  slots_.resize(capacity);
  for (size_t i = 0; i < slots_.size(); ++i)
  {
    slots_[i].id = 0;
    slots_[i].tsMs = 0;
    slots_[i].len = 0;
    slots_[i].data = NULL;
    slots_[i].valid = false;
  }
}

MemoryPhotoStore::~MemoryPhotoStore() { clearAll(); }

bool MemoryPhotoStore::begin()
{
  if (mutex_ == NULL)
  {
    mutex_ = xSemaphoreCreateMutex();
  }
  return mutex_ != NULL;
}

bool MemoryPhotoStore::pushOwnedFrame(uint8_t* buf, size_t len, uint64_t tsMs)
{
  if (buf == NULL || len == 0 || slots_.empty())
  {
    return false;
  }
  if (!lock(2000))
  {
    return false;
  }

  Slot& slot = slots_[writeIndex_];
  if (slot.valid && slot.data != NULL)
  {
    free(slot.data);
  }

  slot.id = nextId_++;
  slot.tsMs = tsMs;
  slot.len = len;
  slot.data = buf;
  slot.valid = true;

  writeIndex_ = (writeIndex_ + 1) % slots_.size();
  if (count_ < slots_.size())
  {
    ++count_;
  }

  unlock();
  return true;
}

bool MemoryPhotoStore::getRecent(std::vector<MemoryPhotoMeta>& out,
                                 size_t maxCount)
{
  out.clear();
  if (!lock(2000))
  {
    return false;
  }

  for (size_t i = 0; i < slots_.size(); ++i)
  {
    const Slot& slot = slots_[i];
    if (slot.valid)
    {
      MemoryPhotoMeta meta;
      meta.id = slot.id;
      meta.tsMs = slot.tsMs;
      meta.len = slot.len;
      out.push_back(meta);
    }
  }

  unlock();

  std::sort(out.begin(), out.end(),
            [](const MemoryPhotoMeta& a, const MemoryPhotoMeta& b)
            {
              if (a.tsMs == b.tsMs)
              {
                return a.id > b.id;
              }
              return a.tsMs > b.tsMs;
            });

  if (maxCount > 0 && out.size() > maxCount)
  {
    out.resize(maxCount);
  }
  return true;
}

bool MemoryPhotoStore::cloneFrameById(uint32_t id, uint8_t** outBuf,
                                      size_t* outLen)
{
  if (outBuf == NULL || outLen == NULL)
  {
    return false;
  }
  *outBuf = NULL;
  *outLen = 0;

  if (!lock(2000))
  {
    return false;
  }

  bool ok = false;
  for (size_t i = 0; i < slots_.size(); ++i)
  {
    Slot& slot = slots_[i];
    if (slot.valid && slot.id == id && slot.data != NULL && slot.len > 0)
    {
      uint8_t* copied = (uint8_t*)malloc(slot.len);
      if (copied != NULL)
      {
        memcpy(copied, slot.data, slot.len);
        *outBuf = copied;
        *outLen = slot.len;
        ok = true;
      }
      break;
    }
  }

  unlock();
  return ok;
}

bool MemoryPhotoStore::deleteById(uint32_t id)
{
  if (!lock(2000))
  {
    return false;
  }

  bool removed = false;
  for (size_t i = 0; i < slots_.size(); ++i)
  {
    Slot& slot = slots_[i];
    if (slot.valid && slot.id == id)
    {
      if (slot.data != NULL)
      {
        free(slot.data);
      }
      slot.valid = false;
      slot.data = NULL;
      slot.len = 0;
      slot.id = 0;
      slot.tsMs = 0;
      if (count_ > 0)
      {
        --count_;
      }
      removed = true;
      break;
    }
  }

  unlock();
  return removed;
}

size_t MemoryPhotoStore::clearAll()
{
  if (!lock(2000))
  {
    return 0;
  }

  size_t removed = 0;
  for (size_t i = 0; i < slots_.size(); ++i)
  {
    Slot& slot = slots_[i];
    if (slot.valid)
    {
      if (slot.data != NULL)
      {
        free(slot.data);
      }
      slot.valid = false;
      slot.data = NULL;
      slot.len = 0;
      slot.id = 0;
      slot.tsMs = 0;
      ++removed;
    }
  }
  count_ = 0;
  writeIndex_ = 0;

  unlock();
  return removed;
}

size_t MemoryPhotoStore::size()
{
  if (!lock(2000))
  {
    return 0;
  }
  size_t value = count_;
  unlock();
  return value;
}

bool MemoryPhotoStore::lock(uint32_t timeoutMs)
{
  return (mutex_ != NULL) &&
         (xSemaphoreTake(mutex_, pdMS_TO_TICKS(timeoutMs)) == pdTRUE);
}

void MemoryPhotoStore::unlock()
{
  if (mutex_ != NULL)
  {
    xSemaphoreGive(mutex_);
  }
}