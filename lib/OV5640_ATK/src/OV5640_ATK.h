#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <stddef.h>
#include <stdint.h>

namespace ov5640_atk
{

struct RegVal
{
  uint16_t reg;
  uint8_t val;
};

// 7-bit SCCB/I2C address for OV5640 (0x78/0x79 in 8-bit form)
constexpr uint8_t kDefaultI2cAddr = 0x3C;
constexpr uint16_t kChipIdExpected = 0x5640;

bool writeReg16(TwoWire& wire, uint8_t addr7, uint16_t reg, uint8_t val);
bool readReg16(TwoWire& wire, uint8_t addr7, uint16_t reg, uint8_t& val);
uint16_t readChipId(TwoWire& wire, uint8_t addr7 = kDefaultI2cAddr);

bool writeTable(TwoWire& wire, uint8_t addr7, const RegVal* table,
                size_t table_len);

// Tables (generated from the STM32 project)
extern const RegVal ov5640_init_cfg[];
extern const size_t ov5640_init_cfg_len;
extern const RegVal ov5640_rgb565_cfg[];
extern const size_t ov5640_rgb565_cfg_len;
extern const RegVal ov5640_jpeg_cfg[];
extern const size_t ov5640_jpeg_cfg_len;

// Apply common table sets
inline bool applyInit(TwoWire& wire, uint8_t addr7 = kDefaultI2cAddr)
{
  return writeTable(wire, addr7, ov5640_init_cfg, ov5640_init_cfg_len);
}
inline bool applyRgb565(TwoWire& wire, uint8_t addr7 = kDefaultI2cAddr)
{
  return writeTable(wire, addr7, ov5640_rgb565_cfg, ov5640_rgb565_cfg_len);
}
inline bool applyJpeg(TwoWire& wire, uint8_t addr7 = kDefaultI2cAddr)
{
  return writeTable(wire, addr7, ov5640_jpeg_cfg, ov5640_jpeg_cfg_len);
}

// Autofocus firmware (optional)
extern const uint8_t ov5640_af_firmware[];
extern const size_t ov5640_af_firmware_len;
constexpr uint16_t kAfDownloadAddr = 0x8000;

bool autofocusDownload(TwoWire& wire, uint8_t addr7 = kDefaultI2cAddr,
                       uint16_t timeout_ms = 5000);
bool autofocusOnce(TwoWire& wire, uint8_t addr7 = kDefaultI2cAddr,
                   uint16_t timeout_ms = 5000);
bool autofocusContinuous(TwoWire& wire, uint8_t addr7 = kDefaultI2cAddr,
                         uint16_t timeout_ms = 5000);

}  // namespace ov5640_atk
