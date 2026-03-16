#include "OV5640_ATK.h"

#include "ov5640_atk_tables.h"

namespace ov5640_atk
{

bool writeReg16(TwoWire& wire, uint8_t addr7, uint16_t reg, uint8_t val)
{
  wire.beginTransmission(addr7);
  wire.write(static_cast<uint8_t>(reg >> 8));
  wire.write(static_cast<uint8_t>(reg & 0xFF));
  wire.write(val);
  return wire.endTransmission(true) == 0;
}

bool readReg16(TwoWire& wire, uint8_t addr7, uint16_t reg, uint8_t& val)
{
  wire.beginTransmission(addr7);
  wire.write(static_cast<uint8_t>(reg >> 8));
  wire.write(static_cast<uint8_t>(reg & 0xFF));
  if (wire.endTransmission(false) != 0)
  {
    return false;
  }
  if (wire.requestFrom(static_cast<int>(addr7), 1) != 1)
  {
    return false;
  }
  val = wire.read();
  return true;
}

uint16_t readChipId(TwoWire& wire, uint8_t addr7)
{
  uint8_t high = 0;
  uint8_t low = 0;
  if (!readReg16(wire, addr7, 0x300A, high))
  {
    return 0;
  }
  if (!readReg16(wire, addr7, 0x300B, low))
  {
    return 0;
  }
  return (static_cast<uint16_t>(high) << 8) | low;
}

bool writeTable(TwoWire& wire, uint8_t addr7, const RegVal* table,
                size_t table_len)
{
  for (size_t i = 0; i < table_len; i++)
  {
    if (!writeReg16(wire, addr7, table[i].reg, table[i].val))
    {
      return false;
    }
    // Conservative pacing for SCCB/I2C.
    // Most regs do not need delay; keep tiny to avoid WDT issues with huge
    // tables.
    delayMicroseconds(50);
  }
  return true;
}

bool autofocusDownload(TwoWire& wire, uint8_t addr7, uint16_t timeout_ms)
{
  // Ported from atk_mc5640_auto_focus_init()
  if (!writeReg16(wire, addr7, 0x3000, 0x20))
  {
    return false;
  }

  for (size_t i = 0; i < ov5640_af_firmware_len; i++)
  {
    uint16_t reg = static_cast<uint16_t>(kAfDownloadAddr + i);
    if (!writeReg16(wire, addr7, reg, ov5640_af_firmware[i]))
    {
      return false;
    }
  }

  const uint16_t regs_zero[] = {0x3022, 0x3023, 0x3024, 0x3025,
                                0x3026, 0x3027, 0x3028};
  for (uint16_t r : regs_zero)
  {
    if (!writeReg16(wire, addr7, r, 0x00))
    {
      return false;
    }
  }
  if (!writeReg16(wire, addr7, 0x3029, 0x7F))
  {
    return false;
  }
  if (!writeReg16(wire, addr7, 0x3000, 0x00))
  {
    return false;
  }

  uint32_t start = millis();
  while (millis() - start < timeout_ms)
  {
    uint8_t reg3029 = 0;
    if (!readReg16(wire, addr7, 0x3029, reg3029))
    {
      return false;
    }
    if (reg3029 == 0x70)
    {
      return true;
    }
    delay(1);
  }
  return false;
}

bool autofocusOnce(TwoWire& wire, uint8_t addr7, uint16_t timeout_ms)
{
  // Ported from atk_mc5640_auto_focus_once()
  if (!writeReg16(wire, addr7, 0x3022, 0x03))
  {
    return false;
  }

  uint32_t start = millis();
  while (millis() - start < timeout_ms)
  {
    uint8_t reg3029 = 0;
    if (!readReg16(wire, addr7, 0x3029, reg3029))
    {
      return false;
    }
    if (reg3029 == 0x10)
    {
      return true;
    }
    delay(1);
  }

  return false;
}

bool autofocusContinuous(TwoWire& wire, uint8_t addr7, uint16_t timeout_ms)
{
  // Ported from atk_mc5640_auto_focus_continuance()
  auto waitReg3023Zero = [&](uint16_t local_timeout_ms) -> bool
  {
    uint32_t start = millis();
    while (millis() - start < local_timeout_ms)
    {
      uint8_t reg3023 = 0xFF;
      if (!readReg16(wire, addr7, 0x3023, reg3023))
      {
        return false;
      }
      if (reg3023 == 0x00)
      {
        return true;
      }
      delay(1);
    }
    return false;
  };

  if (!writeReg16(wire, addr7, 0x3023, 0x01)) return false;
  if (!writeReg16(wire, addr7, 0x3022, 0x08)) return false;
  if (!waitReg3023Zero(timeout_ms)) return false;

  if (!writeReg16(wire, addr7, 0x3023, 0x01)) return false;
  if (!writeReg16(wire, addr7, 0x3022, 0x04)) return false;
  if (!waitReg3023Zero(timeout_ms)) return false;

  return true;
}

}  // namespace ov5640_atk
