#include "motor/mpu_sensor.h"

#include "LogSwitch.h"

namespace
{
struct I2CBusCandidate
{
  uint8_t sda;
  uint8_t scl;
  const char* name;
};

static bool isMpuAddressAcked()
{
  Wire.beginTransmission(MPU6050_ADDR);
  uint8_t err = Wire.endTransmission();
  return err == 0;
}
}  // namespace

// ========== MPU_Sensor 构造函数 ==========
MPU_Sensor::MPU_Sensor()
    : initialized(false),
      raw_ax(0),
      raw_ay(0),
      raw_az(0),
      raw_gx(0),
      raw_gy(0),
      raw_gz(0)
{
}

// ========== I2C写寄存器 ==========
void MPU_Sensor::writeReg(uint8_t reg, uint8_t data)
{
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(reg);
  Wire.write(data);
  uint8_t err = Wire.endTransmission();
  if (err != 0)
  {
    LOG_PRINTF(LOG_MPU, "[MPU] I2C write failed, reg=0x%02X err=%u\n", reg,
               err);
  }
}

// ========== I2C读寄存器 ==========
uint8_t MPU_Sensor::readReg(uint8_t reg, bool* ok)
{
  if (ok != nullptr)
  {
    *ok = false;
  }

  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(reg);
  uint8_t txErr = Wire.endTransmission(false);
  if (txErr != 0)
  {
    LOG_PRINTF(LOG_MPU, "[MPU] I2C read tx failed, reg=0x%02X err=%u\n", reg,
               txErr);
    return 0xFF;
  }

  size_t got = Wire.requestFrom((uint8_t)MPU6050_ADDR, (uint8_t)1);
  if (got != 1 || Wire.available() < 1)
  {
    LOG_PRINTF(LOG_MPU, "[MPU] I2C read rx failed, reg=0x%02X got=%u\n", reg,
               (unsigned int)got);
    return 0xFF;
  }

  if (ok != nullptr)
  {
    *ok = true;
  }
  return (uint8_t)Wire.read();
}

// ========== I2C连续读多个字节 ==========
bool MPU_Sensor::readBytes(uint8_t reg, uint8_t* data, uint8_t len)
{
  if (data == nullptr || len == 0)
  {
    return false;
  }

  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(reg);
  uint8_t txErr = Wire.endTransmission(false);
  if (txErr != 0)
  {
    LOG_PRINTF(LOG_MPU, "[MPU] I2C burst tx failed, reg=0x%02X err=%u\n", reg,
               txErr);
    return false;
  }

  size_t got = Wire.requestFrom((uint8_t)MPU6050_ADDR, len);
  if (got != len)
  {
    LOG_PRINTF(LOG_MPU,
               "[MPU] I2C burst rx short, reg=0x%02X expect=%u got=%u\n", reg,
               len, (unsigned int)got);
    return false;
  }

  for (uint8_t i = 0; i < len; i++)
  {
    if (Wire.available() < 1)
    {
      LOG_PRINTF(LOG_MPU, "[MPU] I2C burst rx empty at index=%u, reg=0x%02X\n",
                 i, reg);
      return false;
    }
    data[i] = Wire.read();
  }

  return true;
}

// ========== 初始化MPU6050传感器 ==========
bool MPU_Sensor::init()
{
  LOG_PRINTLN(LOG_MPU, "[MPU] ========== MPU6050初始化开始 ==========");

  // 1. 初始化I2C总线并尝试探测MPU地址
  I2CBusCandidate candidates[2] = {
      {MPU_I2C_SDA_PIN, MPU_I2C_SCL_PIN, "Primary MPU bus"},
      {CAMERA_SCCB_SDA_PIN, CAMERA_SCCB_SCL_PIN, "Camera SCCB fallback"},
  };
  const int candidateCount = MPU_I2C_TRY_CAMERA_SCCB ? 2 : 1;

  bool busReady = false;
  for (int i = 0; i < candidateCount; ++i)
  {
    LOG_PRINTF(LOG_MPU, "[MPU] 探测I2C总线: %s (SDA=GPIO%d, SCL=GPIO%d)\n",
               candidates[i].name, candidates[i].sda, candidates[i].scl);

    Wire.begin(candidates[i].sda, candidates[i].scl);
    Wire.setClock(MPU_I2C_CLOCK_HZ);
    delay(10);

    if (isMpuAddressAcked())
    {
      LOG_PRINTF(LOG_MPU, "[MPU] 在总线 %s 上探测到设备 0x%02X\n",
                 candidates[i].name, MPU6050_ADDR);
      busReady = true;
      break;
    }

    LOG_PRINTF(LOG_MPU, "[MPU] 总线 %s 未探测到设备 0x%02X\n",
               candidates[i].name, MPU6050_ADDR);
  }

  if (!busReady)
  {
    LOG_PRINTLN(LOG_MPU, "[MPU] 错误: I2C总线未检测到MPU6050应答");
    LOG_PRINTLN(LOG_MPU,
                "[MPU] 请检查供电/GND、SDA/SCL接线和模块地址(0x68/0x69)");
    return false;
  }

  LOG_PRINTF(LOG_MPU, "[MPU] I2C总线初始化完成, 时钟: %uHz\n",
             MPU_I2C_CLOCK_HZ);

  // 2. 读取WHO_AM_I寄存器验证设备
  LOG_PRINTLN(LOG_MPU, "[MPU] 读取WHO_AM_I寄存器...");
  bool readOk = false;
  uint8_t whoAmI = readReg(MPU6050_WHO_AM_I, &readOk);
  if (!readOk)
  {
    LOG_PRINTLN(LOG_MPU, "[MPU] 读取WHO_AM_I失败, I2C通信异常");
    return false;
  }
  LOG_PRINTF(LOG_MPU, "[MPU] WHO_AM_I = 0x%02X\n", whoAmI);

  if (whoAmI != 0x68)
  {
    LOG_PRINTLN(LOG_MPU, "[MPU] 错误: MPU6050连接失败!");
    LOG_PRINTLN(LOG_MPU, "[MPU] 请检查:");
    LOG_PRINTLN(LOG_MPU, "[MPU]   1. 硬件连接是否正确");
    LOG_PRINTF(LOG_MPU, "[MPU]   2. SDA/GPIO%d, SCL/GPIO%d 是否接反\n",
               MPU_I2C_SDA_PIN, MPU_I2C_SCL_PIN);
    LOG_PRINTLN(LOG_MPU, "[MPU]   3. 传感器供电是否正常 (3.3V)");
    return false;
  }
  LOG_PRINTLN(LOG_MPU, "[MPU] MPU6050连接成功!");

  // 3. 配置传感器
  LOG_PRINTLN(LOG_MPU, "[MPU] 配置传感器参数...");

  // 唤醒传感器 (PWR_MGMT_1: 取消睡眠,选择内部8MHz时钟)
  writeReg(MPU6050_PWR_MGMT_1, 0x00);
  LOG_PRINTLN(LOG_MPU, "[MPU]   - 唤醒传感器");

  // 采样率分频器: SMPLRT_DIV = 4 (采样率 = 8kHz/(1+4) = 1kHz)
  writeReg(MPU6050_SMPLRT_DIV, 0x04);
  LOG_PRINTLN(LOG_MPU, "[MPU]   - 采样率: 1kHz");

  // 配置: 低通滤波 42Hz
  writeReg(MPU6050_CONFIG, 0x03);
  LOG_PRINTLN(LOG_MPU, "[MPU]   - 低通滤波: 42Hz");

  // 陀螺仪配置: ±250°/s
  writeReg(MPU6050_GYRO_CONFIG, 0x00);
  LOG_PRINTLN(LOG_MPU, "[MPU]   - 陀螺仪量程: ±250°/s");

  // 加速度计配置: ±2g
  writeReg(MPU6050_ACCEL_CONFIG, 0x00);
  LOG_PRINTLN(LOG_MPU, "[MPU]   - 加速度计量程: ±2g");

  initialized = true;
  LOG_PRINTLN(LOG_MPU, "[MPU] ========== MPU6050初始化完成 ==========");
  return true;
}

// ========== 更新传感器数据 ==========
void MPU_Sensor::update()
{
  if (!initialized)
  {
    LOG_PRINTLN(LOG_MPU, "[MPU] 警告: 传感器未初始化,无法更新数据");
    return;
  }

  // 从ACCEL_XOUT_H开始连续读取14个字节
  uint8_t data[14];
  if (!readBytes(MPU6050_ACCEL_XOUT_H, data, 14))
  {
    LOG_PRINTLN(LOG_MPU, "[MPU] 读取传感器数据失败");
    return;
  }

  // 组合高字节和低字节
  raw_ax = (int16_t)(data[0] << 8 | data[1]);
  raw_ay = (int16_t)(data[2] << 8 | data[3]);
  raw_az = (int16_t)(data[4] << 8 | data[5]);
  raw_gx = (int16_t)(data[8] << 8 | data[9]);
  raw_gy = (int16_t)(data[10] << 8 | data[11]);
  raw_gz = (int16_t)(data[12] << 8 | data[13]);
}

// ========== 跌倒检测 ==========
bool MPU_Sensor::checkFall(float threshold)
{
  if (!initialized)
  {
    return false;
  }

  // 转换为g单位 (±2g量程, 16384 LSB/g)
  float ax_g = raw_ax / 16384.0;
  float ay_g = raw_ay / 16384.0;
  float az_g = raw_az / 16384.0;

  // 计算合加速度平方
  float accel_sq = ax_g * ax_g + ay_g * ay_g + az_g * az_g;

  // 串口输出调试信息
  // Serial.print("[MPU] 加速度平方: ");
  // Serial.print(accel_sq);
  // Serial.print(" (阈值: ");
  // Serial.print(threshold);
  // Serial.print(") ");
  if (accel_sq >= threshold)
  {
    LOG_PRINTLN(LOG_MPU, "-> 超过阈值!");
  }
  else
  {
    // Serial.println("-> 正常");
  }

  return accel_sq >= threshold;
}

// ========== 获取加速度计数据 ==========
void MPU_Sensor::getAccel(float* ax, float* ay, float* az)
{
  *ax = raw_ax / 16384.0;
  *ay = raw_ay / 16384.0;
  *az = raw_az / 16384.0;
}

// ========== 获取陀螺仪数据 ==========
void MPU_Sensor::getGyro(float* gx, float* gy, float* gz)
{
  *gx = raw_gx / 131.0;
  *gy = raw_gy / 131.0;
  *gz = raw_gz / 131.0;
}

// ========== 获取全部6轴数据 ==========
void MPU_Sensor::getMotion6(float* ax, float* ay, float* az, float* gx,
                            float* gy, float* gz)
{
  getAccel(ax, ay, az);
  getGyro(gx, gy, gz);
}
