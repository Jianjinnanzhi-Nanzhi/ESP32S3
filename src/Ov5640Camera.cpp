#include "../include/Ov5640Camera.h"

#include "OV5640_ATK.h"

#include <limits>

namespace
{
  constexpr uint8_t kPacketMagic[4] = {'I', 'M', 'G', '0'};

  uint16_t crc16_ccitt(const uint8_t* data, size_t len)
  {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i)
    {
      crc ^= static_cast<uint16_t>(data[i]) << 8;
      for (uint8_t bit = 0; bit < 8; ++bit)
      {
        if (crc & 0x8000)
        {
          crc = static_cast<uint16_t>((crc << 1) ^ 0x1021);
        }
        else
        {
          crc <<= 1;
        }
      }
    }
    return crc;
  }
}

namespace ov5640
{
  Ov5640Camera::Ov5640Camera()
  {
    camera_cfg_.ledc_channel = LEDC_CHANNEL_0;
    camera_cfg_.ledc_timer = LEDC_TIMER_0;

    camera_cfg_.pin_d0 = 11;
    camera_cfg_.pin_d1 = 9;
    camera_cfg_.pin_d2 = 8;
    camera_cfg_.pin_d3 = 10;
    camera_cfg_.pin_d4 = 12;
    camera_cfg_.pin_d5 = 18;
    camera_cfg_.pin_d6 = 17;
    camera_cfg_.pin_d7 = 16;
    camera_cfg_.pin_xclk = 15;
    camera_cfg_.pin_pclk = 13;
    camera_cfg_.pin_vsync = 6;
    camera_cfg_.pin_href = 7;
    camera_cfg_.pin_sccb_sda = 4;
    camera_cfg_.pin_sccb_scl = 5;
    camera_cfg_.pin_pwdn = -1;
    camera_cfg_.pin_reset = -1;

    camera_cfg_.xclk_freq_hz = 20000000;
    camera_cfg_.frame_size = FRAMESIZE_QVGA;
    camera_cfg_.pixel_format = PIXFORMAT_JPEG;
    camera_cfg_.jpeg_quality = 10;
    camera_cfg_.fb_count = 2;
    camera_cfg_.fb_location = CAMERA_FB_IN_PSRAM;
    camera_cfg_.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  }

  Ov5640Camera::Ov5640Camera(Pins req_Pins)
  {
    camera_cfg_.ledc_channel = LEDC_CHANNEL_0;
    camera_cfg_.ledc_timer = LEDC_TIMER_0;

    camera_cfg_.pin_d0 = req_Pins.d0_;
    camera_cfg_.pin_d1 = req_Pins.d1_;
    camera_cfg_.pin_d2 = req_Pins.d2_;
    camera_cfg_.pin_d3 = req_Pins.d3_;
    camera_cfg_.pin_d4 = req_Pins.d4_;
    camera_cfg_.pin_d5 = req_Pins.d5_;
    camera_cfg_.pin_d6 = req_Pins.d6_;
    camera_cfg_.pin_d7 = req_Pins.d7_;
    camera_cfg_.pin_xclk = req_Pins.xclk_;
    camera_cfg_.pin_pclk = req_Pins.pclk_;
    camera_cfg_.pin_vsync = req_Pins.vsync_;
    camera_cfg_.pin_href = req_Pins.href_;
    camera_cfg_.pin_sccb_sda = req_Pins.siod_;
    camera_cfg_.pin_sccb_scl = req_Pins.sioc_;
    camera_cfg_.pin_pwdn = req_Pins.pwdn_;
    camera_cfg_.pin_reset = req_Pins.reset_;

    camera_cfg_.xclk_freq_hz = 20000000;
    camera_cfg_.frame_size = FRAMESIZE_VGA;
    camera_cfg_.pixel_format = PIXFORMAT_JPEG;
    camera_cfg_.jpeg_quality = 10;
    camera_cfg_.fb_count = 2;
    camera_cfg_.fb_location = CAMERA_FB_IN_PSRAM;
    camera_cfg_.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  }

  bool Ov5640Camera::init()
  {
    esp_err_t err = esp_camera_init(&camera_cfg_);
    if (err != ESP_OK)
    {
      Serial.printf("esp_camera_init failed: 0x%x\n", (unsigned)err);
      return false;
    }

    Wire.begin(camera_cfg_.pin_sccb_sda, camera_cfg_.pin_sccb_scl, 400000);

    uint16_t id = ov5640_atk::readChipId(Wire);
    Serial.printf("OV5640 chip id: 0x%04X\n", id);

    ov5640_atk::applyInit(Wire);
    ov5640_atk::applyJpeg(Wire);

    Serial.println("Camera ready");

    return true;
  }

  bool Ov5640Camera::run()
  {
    static uint32_t frame_seq = 0;

    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb)
    {
      Serial.println("esp_camera_fb_get failed");
      return false;
    }

    if (fb->format != PIXFORMAT_JPEG)
    {
      Serial.printf("unexpected frame format: %d\n", (int)fb->format);
      esp_camera_fb_return(fb);
      return false;
    }

    const uint8_t* jpeg = fb->buf;
    const size_t len = fb->len;
    size_t start = std::numeric_limits<size_t>::max();
    size_t end = std::numeric_limits<size_t>::max();

    // Keep only a full JPEG payload between SOI(FFD8) and EOI(FFD9).
    for (size_t i = 0; i + 1 < len; ++i)
    {
      if (jpeg[i] == 0xFF && jpeg[i + 1] == 0xD8)
      {
        start = i;
        break;
      }
    }

    if (start == std::numeric_limits<size_t>::max())
    {
      Serial.println("jpeg soi not found");
      esp_camera_fb_return(fb);
      return false;
    }

    for (size_t i = start; i + 1 < len; ++i)
    {
      if (jpeg[i] == 0xFF && jpeg[i + 1] == 0xD9)
      {
        end = i;
      }
    }

    if (end == std::numeric_limits<size_t>::max())
    {
      Serial.println("jpeg eoi not found");
      esp_camera_fb_return(fb);
      return false;
    }

    const size_t jpeg_len = end - start + 2;
    const uint32_t payload_len = static_cast<uint32_t>(jpeg_len);
    const uint32_t seq = frame_seq++;
    const uint16_t crc = crc16_ccitt(jpeg + start, jpeg_len);

    // Packet layout: magic(4) + len(4 LE) + seq(4 LE) + jpeg + crc16(2 LE)
    Serial.write(kPacketMagic, sizeof(kPacketMagic));
    Serial.write(reinterpret_cast<const uint8_t*>(&payload_len), sizeof(payload_len));
    Serial.write(reinterpret_cast<const uint8_t*>(&seq), sizeof(seq));
    Serial.write(jpeg + start, jpeg_len);
    Serial.write(reinterpret_cast<const uint8_t*>(&crc), sizeof(crc));
    Serial.flush();

    esp_camera_fb_return(fb);
    return true;
  }
}
