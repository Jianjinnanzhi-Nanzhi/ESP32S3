#include "motor/ble_mpu.h"

#include "LogSwitch.h"

// ========== BLE_MPU 构造函数 ==========
BLE_MPU::BLE_MPU()
    : connected(false),
      dataCallback(nullptr),
      pServer(nullptr),
      pTxCharacteristic(nullptr),
      pRxCharacteristic(nullptr),
      lastRxTime(0)
{
  rxBuffer = "";
}

// ========== BLE服务端连接回调 ==========
void MyServerCallbacks::onConnect(BLEServer* pServer)
{
  LOG_PRINTLN(LOG_BLE, "[BLE] 客户端连接事件");
  bleMpu->onConnect();
}

// ========== BLE服务端断开连接回调 ==========
void MyServerCallbacks::onDisconnect(BLEServer* pServer)
{
  LOG_PRINTLN(LOG_BLE, "[BLE] 客户端断开连接事件");
  bleMpu->onDisconnect();
  LOG_PRINTLN(LOG_BLE, "[BLE] 重新开始广播...");
  pServer->startAdvertising();
}

// ========== BLE特征值写入回调 ==========
void MyCallbacks::onWrite(BLECharacteristic* pCharacteristic)
{
  std::string value = pCharacteristic->getValue();
  if (value.length() > 0)
  {
    String data = String(value.c_str());
    LOG_PRINTF(LOG_BLE, "[BLE] 收到写入数据, 长度: %d\n", value.length());
    LOG_PRINT(LOG_BLE, "[BLE] 数据片段: ");
    LOG_PRINTLN(LOG_BLE, data);
    bleMpu->onDataReceived(data);
  }
}

// ========== 处理连接事件 ==========
void BLE_MPU::onConnect()
{
  connected = true;
  rxBuffer = "";  // 清空缓冲区
  lastRxTime = 0;
  LOG_PRINTLN(LOG_BLE, "[BLE] ========== 客户端已连接 ==========");
}

// ========== 处理断开连接事件 ==========
void BLE_MPU::onDisconnect()
{
  connected = false;
  rxBuffer = "";  // 清空缓冲区
  LOG_PRINTLN(LOG_BLE, "[BLE] ========== 客户端已断开 ==========");
}

// ========== 处理收到的数据 ==========
void BLE_MPU::onDataReceived(String data)
{
  // 将数据追加到缓冲区
  rxBuffer += data;
  lastRxTime = millis();

  LOG_PRINT(LOG_BLE, "[BLE] 当前缓冲区: ");
  LOG_PRINTLN(LOG_BLE, rxBuffer);
  LOG_PRINTF(LOG_BLE, "[BLE] 缓冲区长度: %d\n", rxBuffer.length());
}

// ========== 处理接收缓冲区 ==========
// 检测完整数据包并处理
void BLE_MPU::processRxBuffer()
{
  // 检查缓冲区是否为空
  if (rxBuffer.length() == 0)
  {
    return;
  }

  LOG_PRINT(LOG_BLE, "[BLE] 处理缓冲区数据, 长度: ");
  LOG_PRINTLN(LOG_BLE, rxBuffer.length());
  LOG_PRINT(LOG_BLE, "[BLE] 完整数据: ");
  LOG_PRINTLN(LOG_BLE, rxBuffer);

  // 调用用户回调函数处理完整数据
  if (dataCallback != nullptr)
  {
    LOG_PRINTLN(LOG_BLE, "[BLE] 调用用户回调函数");
    dataCallback(rxBuffer);
  }

  // 清空缓冲区
  rxBuffer = "";
}

// ========== tick函数 - 需要定期调用 ==========
// 用于处理分包超时
void BLE_MPU::tick()
{
  if (rxBuffer.length() > 0 && millis() - lastRxTime > 100)
  {
    // 超过100ms没有新数据,认为数据包完整
    LOG_PRINTLN(LOG_BLE, "[BLE] ---------- 超时,认为数据包完整 ----------");
    processRxBuffer();
    LOG_PRINTLN(LOG_BLE, "[BLE] ----------------------------------------");
  }
}

// ========== 初始化BLE ==========
void BLE_MPU::init(const char* deviceName)
{
  LOG_PRINTLN(LOG_BLE, "[BLE] 初始化BLE设备...");

  BLEDevice::init(deviceName);
  LOG_PRINTF(LOG_BLE, "[BLE] 设备名设置为: %s\n", deviceName);

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks(this));
  LOG_PRINTLN(LOG_BLE, "[BLE] BLE服务器创建完成");

  BLEService* pService = pServer->createService(SERVICE_UUID);
  LOG_PRINTLN(LOG_BLE, "[BLE] BLE服务创建完成, UUID: " SERVICE_UUID);

  pTxCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_TX_UUID,
      BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
  pTxCharacteristic->addDescriptor(new BLE2902());
  LOG_PRINTLN(LOG_BLE, "[BLE] TX特征值创建完成");

  pRxCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_RX_UUID,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  pRxCharacteristic->setCallbacks(new MyCallbacks(this));
  LOG_PRINTLN(LOG_BLE, "[BLE] RX特征值创建完成");

  pService->start();
  LOG_PRINTLN(LOG_BLE, "[BLE] BLE服务已启动");

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  LOG_PRINTLN(LOG_BLE, "[BLE] 开始广播,等待客户端连接...");
  LOG_PRINTF(LOG_BLE, "[BLE] 初始化完成, 设备名: %s\n", deviceName);
}

// ========== 发送普通消息 ==========
void BLE_MPU::sendMessage(const String& message)
{
  if (connected && pTxCharacteristic != nullptr)
  {
    LOG_PRINT(LOG_BLE, "[BLE] 发送消息: ");
    LOG_PRINTLN(LOG_BLE, message);
    pTxCharacteristic->setValue(message.c_str());
    pTxCharacteristic->notify();
  }
  else if (!connected)
  {
    LOG_PRINTLN(LOG_BLE, "[BLE] 未连接,无法发送消息");
  }
}

// ========== 发送传感器数据 ==========
void BLE_MPU::sendSensorData(float ax, float ay, float az, float gx, float gy,
                             float gz)
{
  if (connected && pTxCharacteristic != nullptr)
  {
    char buffer[128];
    snprintf(buffer, sizeof(buffer),
             "DATA:ax=%.3f,ay=%.3f,az=%.3f,gx=%.3f,gy=%.3f,gz=%.3f", ax, ay, az,
             gx, gy, gz);
    LOG_PRINT(LOG_BLE, "[BLE] 发送传感器数据: ");
    LOG_PRINTLN(LOG_BLE, buffer);
    pTxCharacteristic->setValue(buffer);
    pTxCharacteristic->notify();
  }
  else if (!connected)
  {
    LOG_PRINTLN(LOG_BLE, "[BLE] 未连接,无法发送传感器数据");
  }
}

// ========== 发送跌倒告警 ==========
void BLE_MPU::sendFallAlert()
{
  if (connected && pTxCharacteristic != nullptr)
  {
    LOG_PRINTLN(LOG_BLE, "[BLE] 发送跌倒告警: EMERGENCY");
    pTxCharacteristic->setValue("EMERGENCY");
    pTxCharacteristic->notify();
  }
  else if (!connected)
  {
    LOG_PRINTLN(LOG_BLE, "[BLE] 未连接,无法发送跌倒告警");
  }
  LOG_PRINTLN(LOG_BLE, "[BLE] 跌倒告警处理完成");
}

// ========== 发送电量数据 ==========
void BLE_MPU::sendBattery(int level)
{
  if (connected && pTxCharacteristic != nullptr)
  {
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%d", level);
    LOG_PRINT(LOG_BLE, "[BLE] 发送电量: ");
    LOG_PRINTLN(LOG_BLE, buffer);
    pTxCharacteristic->setValue(buffer);
    pTxCharacteristic->notify();
  }
  else if (!connected)
  {
    LOG_PRINTLN(LOG_BLE, "[BLE] 未连接,无法发送电量数据");
  }
}

// ========== 获取连接状态 ==========
bool BLE_MPU::isConnected() { return connected; }

// ========== 设置数据接收回调 ==========
void BLE_MPU::setCallback(void (*callback)(String))
{
  dataCallback = callback;
  LOG_PRINTLN(LOG_BLE, "[BLE] 用户回调函数已设置");
}
