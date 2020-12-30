#include <Arduino.h>
#include <Time.h>
#include <WiFi.h>
extern "C" {
#include <crypto/base64.h>
}
#include <NTPClient.h>
#include <PubSubClient.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <ArduinoJson.h>
#include <AnimatedGIF.h>
#include "FS.h"
#include "SPIFFS.h"

#include <Credentials.h>

// Generic
char buffer[80];

// NTP
const uint32_t ntp_update_interval_secs = 3600; // 1h

WiFiUDP ntp_UDP;
NTPClient time_client(ntp_UDP);
unsigned long ntp_last_update = 0;

WiFiClient wifi_client;

// MQTT
const IPAddress mqtt_server(192, 168, 1, 220);
const char* announce_topic = "comms_panel_announce";
const char* control_topic = "comms_panel_control";

PubSubClient mqtt_client;
char message[40];
bool messsage_changed = false;

// SPIFFS
#define FORMAT_SPIFFS_IF_FAILED true

// Matrix
const uint16_t matrix_width = 64;
const uint16_t matrix_height = 64;

MatrixPanel_I2S_DMA matrix;
GFXcanvas16 overlay_canvas(matrix_width, matrix_height); 
unsigned long matrix_last_update = 0;
const uint8_t DEFAULT_MATRIX_BRIGHTNESS = matrix_width / 2;

// AnimatedGIF
AnimatedGIF gif;
File gif_file;
unsigned long start_tick = 0;
bool gif_loaded = false;
bool screen_dirty = true;

// Config
const char* CONFIG_FILENAME = "/config.txt";
const char* GIF_PATH = "/gifs";
const char* UPLOAD_GIF_NAME = "_CUSTOM_";

const bool DEFAULT_SHOW_TIME = true;
const char* DEFAULT_COLOR_TIME = "0xFFEE00";
const bool DEFAULT_SHOW_DATE = true;
const char* DEFAULT_COLOR_DATE = "0xFF0000";
const bool DEFAULT_SCREEN_OFF = false;
const char* DEFAULT_COLOR_MESSAGE = "0xFF6A00";
const char* DEFAULT_GIF_NAME = "alien";

struct Config {
  int brightness;
  bool show_time;
  uint16_t color_time;
  bool show_date;
  uint16_t color_date;
  bool screen_off;
  uint16_t color_message;
  char gif_name[20];
};
Config config;      

// WiFi Functions

String translateEncryptionType(wifi_auth_mode_t encryptionType) {
  switch (encryptionType) {
    case (WIFI_AUTH_OPEN):
      return "Open";
    case (WIFI_AUTH_WEP):
      return "WEP";
    case (WIFI_AUTH_WPA_PSK):
      return "WPA_PSK";
    case (WIFI_AUTH_WPA2_PSK):
      return "WPA2_PSK";
    case (WIFI_AUTH_WPA_WPA2_PSK):
      return "WPA_WPA2_PSK";
    case (WIFI_AUTH_WPA2_ENTERPRISE):
      return "WPA2_ENTERPRISE";
    case (WIFI_AUTH_MAX):
      return "MAX";
  }
  return "UNKNOWN";
}
 
void scanNetworks() {
  int numberOfNetworks = WiFi.scanNetworks();
  Serial.print(F("Number of networks found: "));
  Serial.println(numberOfNetworks);
  for (int i = 0; i < numberOfNetworks; i++) {
    Serial.print(F("Network name: "));
    Serial.println(WiFi.SSID(i));
    Serial.print(F("Signal strength: "));
    Serial.println(WiFi.RSSI(i));
    Serial.print(F("MAC address: "));
    Serial.println(WiFi.BSSIDstr(i));
    Serial.print(F("Encryption type: "));
    String encryptionTypeDescription = translateEncryptionType(WiFi.encryptionType(i));
    Serial.println(encryptionTypeDescription);
    Serial.println(F("-----------------------"));
  }
}
 
void connectToNetwork() {
  WiFi.begin(WLAN_IOT_SSID, WLAN_IOT_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println(F("Establishing connection to WiFi.."));
  }
  Serial.println(F("Connected to network"));
}

// SPIFFS Functions

void list_dir(fs::FS &fs, const char* dirname, uint8_t levels){
  Serial.printf("Listing directory: %s\r\n", dirname);
  File root = fs.open(dirname);
  if(!root){
    Serial.println(F("- failed to open directory"));
    return;
  }
  if(!root.isDirectory()){
    Serial.println(F(" - not a directory"));
    return;
  }
  File file = root.openNextFile();
  while(file){
    if(file.isDirectory()){
      Serial.print(F("  DIR : "));
      Serial.println(file.name());
      if(levels){
        list_dir(fs, file.name(), levels -1);
      }
    } else {
      Serial.print(F("  FILE: "));
      Serial.print(file.name());
      Serial.print(F("\tSIZE: "));
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void read_file(fs::FS &fs, const char* path){
    Serial.printf("Reading file: %s\r\n", path);
    File file = fs.open(path);
    if (!file || file.isDirectory()){
      Serial.println(F("- failed to open file for reading"));
      return;
    }
    Serial.println(F("- read from file:"));
    while(file.available()) {
      Serial.write(file.read());
    }
    file.close();
}

void write_file(fs::FS &fs, const char* path, const char* message) {
    Serial.printf("Writing file: %s\r\n", path);
    File file = fs.open(path, FILE_WRITE);
    if (!file){
      Serial.println(F("- failed to open file for writing"));
      return;
    }
    if (file.print(message)){
      Serial.println(F("- file written"));
    } else {
      Serial.println(F("- write failed"));
    }
    file.close();
}

void write_binary_file(fs::FS &fs, const char* path, const uint8_t* data, size_t size) {
    Serial.printf("Writing binary file: %s\r\n", path);
    File file = fs.open(path, FILE_WRITE);
    if (!file){
      Serial.println(F("- failed to open file for writing"));
      return;
    }
    if (file.write(data, size)){
      Serial.println(F("- file written"));
    } else {
      Serial.println(F("- write failed"));
    }
    file.close();
}

void append_file(fs::FS &fs, const char* path, const char * message) {
    Serial.printf("Appending to file: %s\r\n", path);
    File file = fs.open(path, FILE_APPEND);
    if (!file){
      Serial.println(F("- failed to open file for appending"));
      return;
    }
    if (file.print(message)){
      Serial.println(F("- message appended"));
    } else {
      Serial.println(F("- append failed"));
    }
    file.close();
}

void rename_file(fs::FS &fs, const char* path1, const char* path2) {
  Serial.printf("Renaming file %s to %s\r\n", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println(F("- file renamed"));
  } else {
    Serial.println(F("- rename failed"));
  }
}

void delete_file(fs::FS &fs, const char* path) {
  Serial.printf("Deleting file: %s\r\n", path);
  if(fs.remove(path)){
    Serial.println(F("- file deleted"));
  } else {
    Serial.println(F("- delete failed"));
  }
}

// Config Functions

void store_payload(const char* msg) {
  strcpy(message, msg);
  messsage_changed = true;
}

void store_gif_from_payload(const char* data, const char* name) {
  size_t raw_len = strlen(data);
  size_t decoded_len;
  uint8_t* decoded = (uint8_t*) base64_decode(
    const_cast<const unsigned char*>(reinterpret_cast<const unsigned char*>(data)),
    raw_len, &decoded_len);
  if (decoded_len > 0) {
    sprintf(buffer, "%s/%s.gif", GIF_PATH, name);
    write_binary_file(SPIFFS, buffer, decoded, decoded_len);
  } else {
    Serial.println(F("Failed to decode GIF from BASE64 encoded data."));
  }
  free(decoded);
}

uint16_t long_to_color(long color) {
  return matrix.color565((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
}

void update_config(const JsonDocument &doc) {
  if (doc.containsKey("message")) {
    store_payload(doc["message"]);
  }
  config.brightness = doc["brightness"] | DEFAULT_MATRIX_BRIGHTNESS;
  config.show_time = doc["show_time"] | DEFAULT_SHOW_TIME;
  config.color_time = long_to_color(std::strtol(doc["color_time"] | DEFAULT_COLOR_TIME, NULL, 0));
  config.show_date = doc["show_date"] | DEFAULT_SHOW_DATE;
  config.color_date = long_to_color(std::strtol(doc["color_date"] | DEFAULT_COLOR_DATE, NULL, 0));
  config.screen_off = doc["screen_off"] | DEFAULT_SCREEN_OFF;
  config.color_message = std::strtol(doc["color_message"] | DEFAULT_COLOR_MESSAGE, NULL, 0);
  strlcpy(config.gif_name, doc["gif_name"] | DEFAULT_GIF_NAME, sizeof(config.gif_name));
  if (doc.containsKey("gif_name")) {
    gif_loaded = false;
  }
  if (doc.containsKey("gif_data")) {
    strlcpy(config.gif_name, UPLOAD_GIF_NAME, sizeof(config.gif_name));
    store_gif_from_payload(doc["gif_data"], UPLOAD_GIF_NAME);
  }
}

void load_config(const char *filename, Config &config) {
  File file = SPIFFS.open(filename);
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.println(F("Failed to read file, using default configuration"));
    read_file(SPIFFS, filename);
  }
  update_config(doc);
  file.close();
}

void save_config(const char* filename, const Config &config) {
  SPIFFS.remove(filename);
  File file = SPIFFS.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println(F("Failed to create file"));
    return;
  }
  StaticJsonDocument<512> doc;
  doc["brightness"] = config.brightness;
  doc["show_time"] = config.show_time;
  doc["color_time"] = config.color_time;
  doc["show_date"] = config.show_date;
  doc["color_date"] = config.color_date;
  doc["screen_off"] = config.screen_off;
  doc["color_message"] = config.color_message;
  doc["gif_name"] = config.gif_name;
  if (serializeJson(doc, file) == 0) {
    Serial.println(F("Failed to write to file"));
  }
  file.close();
}

// MQTT Functions

/*
  Example MQTT message:

  { 
    "msg": "",
    "brightness": 5,
    "show_date": true,
    "show_time": true,
    "screen_off": false
  }
*/

void parse_mqtt_payload(const char* payload) {
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.print(F("Parsing MQTT payload failed: "));
    Serial.println(error.f_str());
    return;
  }
  update_config(doc);
  Serial.println(F("Saving configuration..."));
  save_config(CONFIG_FILENAME, config);
  read_file(SPIFFS, CONFIG_FILENAME);
}

void mqtt_callback(const char* topic, byte* payload, unsigned int length) {
  Serial.print(F("Message arrived ["));
  Serial.print(topic);
  Serial.print(F("] "));
  for (int i = 0; i < length; i++) {
    Serial.print((char) payload[i]);
  }
  Serial.println();
  char* pl = (char*) malloc(length * sizeof(char) + 1);
  memcpy(pl, payload, length);
  pl[length] = 0;
  parse_mqtt_payload(pl);
  free(pl);
}

void reconnect() {
  while (!mqtt_client.connected()) {
    Serial.print(F("Attempting MQTT connection..."));
    if (mqtt_client.connect("arduinoClient")) {
      Serial.println(F("MQTT connected."));
      // mqtt_client.publish(announce_topic, announce_msg);
      mqtt_client.subscribe(control_topic);
      Serial.print(F("Subscribed MQTT topic: "));
      Serial.println(control_topic);
    } else {
      Serial.print(F("MQTT connection failed, rc="));
      Serial.print(mqtt_client.state());
      Serial.println(F(" - try again in 5 seconds"));
      delay(5000);
    }
  }
}

// Matrix Funtions

void draw_centered_text(Adafruit_GFX* canvas, const char* text, int x, 
    bool center_x, int y, bool center_y, int size, uint16_t rgb,
    const GFXfont* font) {
  int16_t  x1, y1, xa, ya;
  uint16_t w, h;

  canvas->setFont(font);
  canvas->setTextSize(size);
  canvas->setTextColor(rgb);
  canvas->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  // Serial.print("x1: "); Serial.print(x1);
  // Serial.print(", y1: "); Serial.print(y1);
  // Serial.print(", w: "); Serial.print(w);
  // Serial.print(", h: "); Serial.println(h);
  xa = x;
  if (center_x) {
    xa = (canvas->width() - (w - 1)) / 2 - x1;
  } else if (x < 0) {
    xa = canvas->width() - (w - 1) + x - x1 - 1;
  }
  ya = y;
  if (center_y) {
    ya = (canvas->height() - (h - 1)) / 2 - y1; 
  } else if (y < 0) {
    ya = canvas->height() - (h - 1) + y - y1 - 1;
  }
  // Serial.print("xa: "); Serial.print(xa);
  // Serial.print(", ya: "); Serial.println(ya);
  canvas->setCursor(xa, ya); 
  canvas->print(text);
}

void draw_overlay() {
  // Clear screen and set some text defaults
  overlay_canvas.setTextSize(1);     // size 1 == 8 pixels high
  overlay_canvas.setTextWrap(false);
  overlay_canvas.fillScreen(matrix.color444(0, 0, 0));

  // MQTT status
  if (!mqtt_client.connected()) {
    overlay_canvas.drawCircle(overlay_canvas.width() - 2, 2, 2, 
      matrix.color565(255, 0, 0));
  }

  // Payload - TODO 
  if (strlen(message) > 0) {
    draw_centered_text(&overlay_canvas, message, -1, true, -1, true, 
      1, config.color_message, &FreeSansBold9pt7b);
    messsage_changed = false;
  }

  // Time and date
  if (config.show_time || config.show_date) {
    time_t t = time_client.getEpochTime();
    if (config.show_date) {
      sprintf(buffer, "%04d/%02d/%02d", year(t), month(t), day(t));
      draw_centered_text(&overlay_canvas, buffer, -1, true, 1, false,
        1, config.color_date, NULL);
    }
    if (config.show_time) {
      sprintf(buffer, "%02d:%02d", hour(t), minute(t));
      draw_centered_text(&overlay_canvas, buffer, -1, true, -1, false, 
        1, config.color_time, &FreeSansBold9pt7b);
    }
  }
}

// AnimatedGIF Functions

// Draw a line of image directly on the LED Matrix
void gif_draw(GIFDRAW *pDraw) {
  uint8_t *s;
  uint16_t *d, *usPalette, usTemp[320];
  int x, y, iWidth;

  iWidth = pDraw->iWidth;
  if (iWidth > MATRIX_WIDTH) iWidth = MATRIX_WIDTH;

  usPalette = pDraw->pPalette;
  y = pDraw->iY + pDraw->y; // current line
  
  s = pDraw->pPixels;
  if (pDraw->ucDisposalMethod == 2) { // restore to background color
    for (x=0; x<iWidth; x++) {
      if (s[x] == pDraw->ucTransparent)
          s[x] = pDraw->ucBackground;
    }
    pDraw->ucHasTransparency = 0;
  }
  // Apply the new pixels to the main image
  if (pDraw->ucHasTransparency) { // if transparency used
    uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
    int x, iCount;
    pEnd = s + pDraw->iWidth;
    x = 0;
    iCount = 0; // count non-transparent pixels
    while (x < pDraw->iWidth) {
      c = ucTransparent-1;
      d = usTemp;
      while (c != ucTransparent && s < pEnd) {
        c = *s++;
        if (c == ucTransparent) { // done, stop
          s--; // back up to treat it like transparent
        } else { // opaque
          *d++ = usPalette[c];
          iCount++;
        }
      }
      if (iCount) { // any opaque pixels?
        for (int xOffset = 0; xOffset < iCount; xOffset++ ){
          matrix.drawPixelRGB565(x + xOffset, y, usTemp[xOffset]);
        }
        x += iCount;
        iCount = 0;
      }
      // no, look for a run of transparent pixels
      c = ucTransparent;
      while (c == ucTransparent && s < pEnd) {
        c = *s++;
        if (c == ucTransparent)
          iCount++;
        else
          s--; 
      }
      if (iCount) {
        x += iCount; // skip these
        iCount = 0;
      }
    }
  } else { // does not have transparency
    s = pDraw->pPixels;
    // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
    for (x = 0; x < pDraw->iWidth; x++) {
      matrix.drawPixelRGB565(x, y, usPalette[*s++]);
    }
  }
  // Apply the overlay - TODO: Should be done inside the above loops
  for (x = 0; x < pDraw->iWidth; x++) {
    uint16_t c = overlay_canvas.getPixel(x, y);
    if (c > 0) {
      matrix.drawPixel(x, y, c);
    }
  }
}

void * gif_open_file(const char *fname, int32_t *pSize) {
  gif_file = SPIFFS.open(fname);
  if (gif_file) {
    *pSize = gif_file.size();
    return (void *)&gif_file;
  }
  return NULL;
}

void gif_close_file(void *pHandle) {
  File *gif_file = static_cast<File *>(pHandle);
  if (gif_file != NULL)
     gif_file->close();
}

int32_t gif_read_file(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
    int32_t iBytesRead;
    iBytesRead = iLen;
    File *gif_file = static_cast<File *>(pFile->fHandle);
    // Note: If you read a file all the way to the last byte, seek() stops working
    if ((pFile->iSize - pFile->iPos) < iLen)
       iBytesRead = pFile->iSize - pFile->iPos - 1; // <-- ugly work-around
    if (iBytesRead <= 0)
       return 0;
    iBytesRead = (int32_t)gif_file->read(pBuf, iBytesRead);
    pFile->iPos = gif_file->position();
    return iBytesRead;
}

int32_t gif_seek_file(GIFFILE *pFile, int32_t iPosition) { 
  int i = micros();
  File *gif_file = static_cast<File *>(pFile->fHandle);
  gif_file->seek(iPosition);
  pFile->iPos = (int32_t)gif_file->position();
  i = micros() - i;
//  Serial.printf("Seek time = %d us\n", i);
  return pFile->iPos;
}

void show_gif_loop(const char* name) {
  start_tick = millis();
  int x_offset, y_offset;
   
  if (gif.open(name, gif_open_file, gif_close_file, gif_read_file, gif_seek_file, gif_draw))   {
    x_offset = (MATRIX_WIDTH - gif.getCanvasWidth())/2;
    if (x_offset < 0) x_offset = 0;
    y_offset = (MATRIX_HEIGHT - gif.getCanvasHeight())/2;
    if (y_offset < 0) y_offset = 0;
    Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
    Serial.flush();
    while (gif.playFrame(true, NULL))     {      
      if ( (millis() - start_tick) > 8000) { // we'll get bored after about 8 seconds of the same looping gif
        break;
      }
    }
    gif.close();
  }
}

void load_gif(const char* name) {
  int x_offset, y_offset;
  sprintf(buffer, "%s/%s.gif", GIF_PATH, name);
  if (gif.open(buffer, gif_open_file, gif_close_file, gif_read_file, gif_seek_file, gif_draw)) {
    x_offset = (MATRIX_WIDTH - gif.getCanvasWidth())/2;
    if (x_offset < 0) x_offset = 0;
    y_offset = (MATRIX_HEIGHT - gif.getCanvasHeight())/2;
    if (y_offset < 0) y_offset = 0;
    Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
    Serial.flush();
    gif_loaded = true;
  } else {
    Serial.printf("Failed to open GIF: %s\n", buffer);
  }
}

// Arduino Functions

void setup() {
  // Generic setup
  Serial.begin(115200);
 
  // SPIFFS setup
  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
      Serial.println(F("SPIFFS Mount Failed"));
      return;
  }
  list_dir(SPIFFS, "/", 0);  
  // SPIFFS.format(); // should go into MQTT handler

  // Load configuration
  Serial.println(F("Loading configuration..."));
  load_config(CONFIG_FILENAME, config);

  // WiFi setup
  scanNetworks();
  connectToNetwork();
 
  Serial.println(WiFi.macAddress());
  Serial.print(F("Acquired IP address: "));
  Serial.println(WiFi.localIP());

  // NTP setup
  time_client.begin();
  time_client.setTimeOffset(3600);

  // MQTT setup
  mqtt_client.setClient(wifi_client);
  mqtt_client.setServer(mqtt_server, 1883);
  mqtt_client.setCallback(mqtt_callback);

  // Matrix setup
  matrix.setPanelBrightness(config.brightness);
  matrix.begin();
  matrix.fillScreen(matrix.color444(0, 0, 0));

  // AnimatedGIF setup  
  gif.begin(LITTLE_ENDIAN_PIXELS);

  // Allow the hardware to sort itself out
  delay(1500);
}
 
void loop() {
  ulong current_secs = time_client.getEpochTime();

  // NTP loop
  if (ntp_last_update == 0 || 
      current_secs - ntp_last_update > ntp_update_interval_secs) {
    Serial.println(F("NTP updating time..."));
    if (ntp_last_update == 0 && !config.screen_off) {
      draw_centered_text(&matrix, "NTP", -1, true, -1, true, 
        1, matrix.color565(0, 0, 255), NULL);
    }
    while (!time_client.update()) {
      Serial.println(F("NTP forcing update..."));
      time_client.forceUpdate();
    }
    if (ntp_last_update == 0) {
        matrix.clearScreen();
    }
    current_secs = time_client.getEpochTime();
    ntp_last_update = current_secs;
    setTime(current_secs);
    Serial.print(F("NTP client updated, current epoch: "));
    Serial.println(current_secs);
  }

  // MQTT loop
  if (!mqtt_client.connected()) {
    reconnect();
  }
  mqtt_client.loop();

  matrix.setPanelBrightness(config.brightness);
  if (!config.screen_off) {
    screen_dirty = true;
    // Matrix loop
    if (messsage_changed || matrix_last_update == 0 ||
        current_secs - matrix_last_update > 1) {
      draw_overlay();
      messsage_changed = false;
      matrix_last_update = current_secs;
    }

    // AnimatedGIF loop
    int delayMillisecs = 0;
    if (gif_loaded) {
      if (gif.playFrame(false, &delayMillisecs) < 0) {
        Serial.println(F("Failed to play GIF frame."));
      }
    } else {
      load_gif(config.gif_name);
    }
    //Serial.printf("Sleeping for (msecs): %d\n", delayMillisecs);
    if (delayMillisecs > 0) {
      delay(delayMillisecs); // sleep till the end of the frame
    }
    // TODO: better loop here and call playFrame in time?
  } else {
    if (screen_dirty) {
      matrix.clearScreen();
      screen_dirty = false;
    } else {
      matrix.drawPixelRGB565(0, 0, matrix.color565(0, 255, 0));
      screen_dirty = true;
    }
    delay(1000); // blink dot per second
  }
}