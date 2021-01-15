#include "Config.h"

ConfigClass::ConfigClass() {
}

void ConfigClass::begin() {
  Debug.println(F("Loading configuration..."));
  load_config(Config.CONFIG_FILENAME, _config);
}

void ConfigClass::update(const JsonDocument &doc) {
  update_config(doc);
  Debug.println(F("Saving _configuration..."));
  save_config(CONFIG_FILENAME, _config);
  Storage.dump_file(CONFIG_FILENAME);
}

void ConfigClass::store_gif_from_payload(const char* data, const char* name) {
  size_t raw_len = strlen(data);
  size_t decoded_len;
  uint8_t* decoded = (uint8_t*) base64_decode(
    const_cast<const unsigned char*>(reinterpret_cast<const unsigned char*>(data)),
    raw_len, &decoded_len);
  if (decoded_len > 0) {
    sprintf(_buffer, "%s/%s.gif", GIF_PATH, name);
    Storage.write_binary_file(_buffer, decoded, decoded_len);
  } else {
    Debug.println(F("Failed to decode GIF from BASE64 encoded data."));
  }
  free(decoded);
}

uint16_t ConfigClass::color565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

uint16_t ConfigClass::long_to_color(long color) {
  return color565((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
}

void ConfigClass::update_config(const JsonDocument &doc) {
  // if (doc.containsKey("message")) {
  //   store_payload(doc["message"]);
  // }
  _config.brightness = doc["brightness"] | DEFAULT_MATRIX_BRIGHTNESS;
  _config.show_gif = doc["show_gif"] | DEFAULT_SHOW_GIF;
  _config.show_time = doc["show_time"] | DEFAULT_SHOW_TIME;
  _config.color_time = long_to_color(std::strtol(doc["color_time"] | DEFAULT_COLOR_TIME, NULL, 0));
  _config.show_date = doc["show_date"] | DEFAULT_SHOW_DATE;
  _config.color_date = long_to_color(std::strtol(doc["color_date"] | DEFAULT_COLOR_DATE, NULL, 0));
  _config.screen_off = doc["screen_off"] | DEFAULT_SCREEN_OFF;
  _config.color_message = std::strtol(doc["color_message"] | DEFAULT_COLOR_MESSAGE, NULL, 0);
  strlcpy(_config.gif_name, doc["gif_name"] | DEFAULT_GIF_NAME, sizeof(_config.gif_name));
  // if (doc.containsKey("gif_name")) {
  //   _gif_loaded = false;
  // }
  if (doc.containsKey("gif_data")) {
    strlcpy(_config.gif_name, UPLOAD_GIF_NAME, sizeof(_config.gif_name));
    store_gif_from_payload(doc["gif_data"], UPLOAD_GIF_NAME);
  }
}

void ConfigClass::load_config(const char *filename, ConfigData &config) {
  char* data = Storage.read_file(filename);
  if (data != NULL) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, data);
    if (error) {
      Debug.println(F("Failed to read file, using default configuration"));
      Storage.dump_file(filename);
    }
    update_config(doc);
    free(data);     
  } else {
    Debug.println(F("Failed to read file, using default configuration"));
  }
}

void ConfigClass::save_config(const char* filename, const ConfigData &config) {
  SPIFFS.remove(filename);
  File file = SPIFFS.open(filename, FILE_WRITE);
  if (!file) {
    Debug.println(F("Failed to create file"));
    return;
  }
  StaticJsonDocument<512> doc;
  doc["brightness"] = _config.brightness;
  doc["show_gif"] = _config.show_gif;
  doc["show_time"] = _config.show_time;
  doc["color_time"] = _config.color_time;
  doc["show_date"] = _config.show_date;
  doc["color_date"] = _config.color_date;
  doc["screen_off"] = _config.screen_off;
  doc["color_message"] = _config.color_message;
  doc["gif_name"] = _config.gif_name;
  if (serializeJson(doc, file) == 0) {
    Debug.println(F("Failed to write to file"));
  }
  file.close();
}

ConfigClass::ConfigData ConfigClass::data() {
  return _config;
}

ConfigClass Config;
