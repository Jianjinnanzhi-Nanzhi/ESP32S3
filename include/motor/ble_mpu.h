#ifndef BLE_MPU_H
#define BLE_MPU_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_RX_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_TX_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a9"

class BLE_MPU {
public:
    BLE_MPU();
    void init(const char* deviceName = "ESP32_MPU6050");
    void sendMessage(const String& message);
    void sendSensorData(float ax, float ay, float az, float gx, float gy, float gz);
    void sendFallAlert();
    void sendBattery(int level);
    bool isConnected();
    void setCallback(void (*callback)(String));

    void onConnect();
    void onDisconnect();
    void onDataReceived(String data);
    void tick();           // 定期调用处理分包超时

private:
    BLEServer* pServer;
    BLECharacteristic* pTxCharacteristic;
    BLECharacteristic* pRxCharacteristic;
    bool connected;
    void (*dataCallback)(String);

    String rxBuffer;              // BLE接收缓冲区
    unsigned long lastRxTime;     // 最后一次接收数据的时间
    void processRxBuffer();       // 处理接收缓冲区
};

class MyServerCallbacks : public BLEServerCallbacks {
public:
    MyServerCallbacks(BLE_MPU* bleMpu) : bleMpu(bleMpu) {}
    void onConnect(BLEServer* pServer) override;
    void onDisconnect(BLEServer* pServer) override;
private:
    BLE_MPU* bleMpu;
};

class MyCallbacks : public BLECharacteristicCallbacks {
public:
    MyCallbacks(BLE_MPU* bleMpu) : bleMpu(bleMpu) {}
    void onWrite(BLECharacteristic* pCharacteristic) override;
private:
    BLE_MPU* bleMpu;
};

#endif
