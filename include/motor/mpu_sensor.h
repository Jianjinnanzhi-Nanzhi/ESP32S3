#ifndef MPU_SENSOR_H
#define MPU_SENSOR_H

#include <Arduino.h>

#include "Wire.h"

// ========== MPU6050 寄存器地址定义 ==========
#define MPU6050_ADDR 0x68          // MPU6050 I2C地址 (AD0=0时为0x68)
#define MPU6050_WHO_AM_I 0x75      // 器件ID寄存器
#define MPU6050_PWR_MGMT_1 0x6B    // 电源管理寄存器1
#define MPU6050_SMPLRT_DIV 0x19    // 采样率分频器
#define MPU6050_CONFIG 0x1A        // 配置寄存器
#define MPU6050_GYRO_CONFIG 0x1B   // 陀螺仪配置
#define MPU6050_ACCEL_CONFIG 0x1C  // 加速度计配置
#define MPU6050_ACCEL_XOUT_H 0x3B  // 加速度计X轴高字节
#define MPU6050_ACCEL_XOUT_L 0x3C  // 加速度计X轴低字节
#define MPU6050_ACCEL_YOUT_H 0x3D  // 加速度计Y轴高字节
#define MPU6050_ACCEL_YOUT_L 0x3E  // 加速度计Y轴低字节
#define MPU6050_ACCEL_ZOUT_H 0x3F  // 加速度计Z轴高字节
#define MPU6050_ACCEL_ZOUT_L 0x40  // 加速度计Z轴低字节
#define MPU6050_GYRO_XOUT_H 0x43   // 陀螺仪X轴高字节
#define MPU6050_GYRO_XOUT_L 0x44   // 陀螺仪X轴低字节
#define MPU6050_GYRO_YOUT_H 0x45   // 陀螺仪Y轴高字节
#define MPU6050_GYRO_YOUT_L 0x46   // 陀螺仪Y轴低字节
#define MPU6050_GYRO_ZOUT_H 0x47   // 陀螺仪Z轴高字节
#define MPU6050_GYRO_ZOUT_L 0x48   // 陀螺仪Z轴低字节

// ========== 引脚定义 ==========
#ifndef MPU_I2C_SDA_PIN
#define MPU_I2C_SDA_PIN 21
#endif

#ifndef MPU_I2C_SCL_PIN
#define MPU_I2C_SCL_PIN 20
#endif

#ifndef MPU_I2C_CLOCK_HZ
#define MPU_I2C_CLOCK_HZ 100000
#endif

#ifndef MPU_I2C_TRY_CAMERA_SCCB
#define MPU_I2C_TRY_CAMERA_SCCB 1
#endif

#define CAMERA_SCCB_SDA_PIN 4
#define CAMERA_SCCB_SCL_PIN 5

class MPU_Sensor
{
 public:
  MPU_Sensor();
  bool init();
  void update();
  bool checkFall(float threshold = 3.0);
  void getAccel(float* ax, float* ay, float* az);
  void getGyro(float* gx, float* gy, float* gz);
  void getMotion6(float* ax, float* ay, float* az, float* gx, float* gy,
                  float* gz);

 private:
  // I2C读写辅助函数
  void writeReg(uint8_t reg, uint8_t data);
  uint8_t readReg(uint8_t reg, bool* ok = nullptr);
  bool readBytes(uint8_t reg, uint8_t* data, uint8_t len);

  int16_t raw_ax, raw_ay, raw_az;
  int16_t raw_gx, raw_gy, raw_gz;
  bool initialized;
};

#endif
