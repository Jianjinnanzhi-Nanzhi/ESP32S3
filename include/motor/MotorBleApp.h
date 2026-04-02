#ifndef MOTOR_BLE_APP_H
#define MOTOR_BLE_APP_H

// Legacy API names kept for compatibility. This module now controls motors
// only.
bool initMotorBleApp();
bool startMotorBleTasks();

bool motorStartByAngle(int angle);
bool motorStop();

#endif
