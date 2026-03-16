#ifndef OV5640CAMERA_H
#define OV5640CAMERA_H

#include <Arduino.h>
#include <Wire.h>

#include "esp_camera.h"

namespace ov5640
{
   class Ov5640Camera
   {
   public:
   struct Pins
   {
      unsigned int pwdn_;
      unsigned int reset_;
      unsigned int xclk_;
      unsigned int siod_;
      unsigned int sioc_;
      unsigned int d0_;
      unsigned int d1_;
      unsigned int d2_;
      unsigned int d3_;
      unsigned int d4_;
      unsigned int d5_;
      unsigned int d6_;
      unsigned int d7_;
      unsigned int vsync_;
      unsigned int href_;
      unsigned int pclk_;
   };
   Ov5640Camera();
   Ov5640Camera(Pins req_Pins);

   bool init();
   bool run();

   private:
   camera_config_t camera_cfg_;
   };
}

#endif // OV5640CAMERA_H