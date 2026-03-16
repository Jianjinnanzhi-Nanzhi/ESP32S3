#include <Arduino.h>

#include "../include/Ov5640Camera.h"

ov5640::Ov5640Camera g_camera;

void setup()
{
  Serial.begin(2000000);
  delay(200);

  if (!g_camera.init())
  {
    while (true) delay(1000);
  }

  Serial.println("READY: send 'C' to capture one JPEG frame");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const int cmd = Serial.read();
    if (cmd == 'C' || cmd == 'c')
    {
      g_camera.run();
    }
  }
  delay(2);
}
