#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <Arduino.h>
#include <ArduinoJson.h>

extern "C" {
#include <crypto/base64.h>
}

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#include "Debug.h"
#include "Storage.h"

class ConfigClass {
  public:
    const char* CONFIG_FILENAME = "/config.txt";
    const char* GIF_PATH = "/gifs";
    const char* UPLOAD_GIF_NAME = "_CUSTOM_";

    typedef struct ConfigFields {
      int brightness;
      bool show_gif;
      bool show_time;
      uint16_t color_time;
      bool show_date;
      uint16_t color_date;
      bool screen_off;
      uint16_t color_message;
      char gif_name[20];
    } ConfigData; 

    ConfigClass();
    void begin();
  
    void update(const JsonDocument &doc);
    ConfigData data();

  private:
    const uint8_t DEFAULT_MATRIX_BRIGHTNESS = MATRIX_WIDTH / 2;
    const bool DEFAULT_SHOW_GIF = true;
    const bool DEFAULT_SHOW_TIME = true;
    const char* DEFAULT_COLOR_TIME = "0xFFEE00";
    const bool DEFAULT_SHOW_DATE = true;
    const char* DEFAULT_COLOR_DATE = "0xFF0000";
    const bool DEFAULT_SCREEN_OFF = false;
    const char* DEFAULT_COLOR_MESSAGE = "0xFF6A00";
    const char* DEFAULT_GIF_NAME = "alien";

    ConfigData _config;
    char _buffer[80];

    void store_payload(const char* msg);
    void store_gif_from_payload(const char* data, const char* name);
    uint16_t long_to_color(long color);
    uint16_t color565(uint8_t r, uint8_t g, uint8_t b);
    void update_config(const JsonDocument &doc);
    void load_config(const char *filename, ConfigData &config);
    void save_config(const char* filename, const ConfigData &config);
};

extern ConfigClass Config;

#endif /* _CONFIG_H_ */