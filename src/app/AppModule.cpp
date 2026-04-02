#include "app/AppModule.h"

#include "WIFI/WifiModule.h"
#include "ble/BleModule.h"
#include "camera/CameraModule.h"
#include "motor/MotorModule.h"
#include "ultrasonic/ultrasonic.h"

namespace esp32s3
{
namespace
{
class BootCoordinator
{
 public:
  static void forwardWifiNotifyToBle(const String& message)
  {
    BleModule::sendMessage(message);
  }

  static bool handleWifiBleBridgeCommand(const String& data)
  {
    return WifiModule::handleBleCommand(data);
  }
};
}  // namespace

bool AppModule::boot()
{
  bool motorInitOk = MotorModule::init();
  bool bleInitOk = BleModule::init();
  bool wifiInitOk = WifiModule::init(BootCoordinator::forwardWifiNotifyToBle);
  bool ultrasonicInitOk =
      UltrasonicModule::init0(ULTRASONIC_TRIG_PIN0, ULTRASONIC_ECHO_PIN0) &&
      UltrasonicModule::init1(ULTRASONIC_TRIG_PIN1, ULTRASONIC_ECHO_PIN1) ;
  bool cameraInitOk = CameraModule::init();

  BleModule::registerExternalCommandHandler(
      BootCoordinator::handleWifiBleBridgeCommand);

  bool motorTaskOk = false;
  bool bleTaskOk = false;
  bool wifiTaskOk = false;
  bool ultrasonicTaskOk = false;
  bool cameraTaskOk = false;

  if (motorInitOk)
  {
    motorTaskOk = MotorModule::startTasks();
  }
  if (bleInitOk)
  {
    bleTaskOk = BleModule::startTasks();
  }
  if (wifiInitOk)
  {
    wifiTaskOk = WifiModule::startTask(1, 0, 4096);
  }
  if (ultrasonicInitOk)
  {
    ultrasonicTaskOk = UltrasonicModule::startTask();
  }
  if (cameraInitOk)
  {
    cameraTaskOk = CameraModule::startTasks();
  }

  return motorInitOk && bleInitOk && wifiInitOk && ultrasonicInitOk &&
         cameraInitOk && motorTaskOk && bleTaskOk && wifiTaskOk &&
         ultrasonicTaskOk && cameraTaskOk;
}
}  // namespace esp32s3
