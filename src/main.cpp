#include <Arduino.h>
#include <Time.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <PubSubClient.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <ArduinoJson.h>
#include <AnimatedGIF.h>
#include "FS.h"
#include "SPIFFS.h"

// NTP Settings
const uint32_t ntp_update_interval_secs = 3600; // 1h

WiFiUDP ntp_UDP;
NTPClient time_client(ntp_UDP);
unsigned long ntp_last_update = 0;

// WiFi Settings
const char* ssid = "Cyclone-IoT";
const char* password =  "9n*2KVn4kkHw";

WiFiClient wifi_client;

// MQTT Settings
const IPAddress mqtt_server(192, 168, 1, 220);
const char* announce_topic = "comms_panel_announce";
const char* control_topic = "comms_panel_control";

PubSubClient mqtt_client;
char msg_payload[40];
bool msg_payload_changed = false;

// SPIFFS Settings
#define FORMAT_SPIFFS_IF_FAILED true

// Matrix Settings
const uint16_t matrix_width = 64;
const uint16_t matrix_height = 64;

MatrixPanel_I2S_DMA dma_display;
GFXcanvas16 overlay_canvas(matrix_width, matrix_height); 
unsigned long matrix_last_update = 0;
uint8_t default_matrix_brightness = matrix_width / 2;

// AnimatedGIF Settings

AnimatedGIF gif;
File gif_file;
unsigned long start_tick = 0;
bool gif_loaded = false;

// Config Settings
const char *config_filename = "/config.txt";

struct Config {
  int matrix_brightness;
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
 
  Serial.print("Number of networks found: ");
  Serial.println(numberOfNetworks);
 
  for (int i = 0; i < numberOfNetworks; i++) {
 
    Serial.print("Network name: ");
    Serial.println(WiFi.SSID(i));
 
    Serial.print("Signal strength: ");
    Serial.println(WiFi.RSSI(i));
 
    Serial.print("MAC address: ");
    Serial.println(WiFi.BSSIDstr(i));
 
    Serial.print("Encryption type: ");
    String encryptionTypeDescription = translateEncryptionType(WiFi.encryptionType(i));
    Serial.println(encryptionTypeDescription);
    Serial.println("-----------------------");
  }
}
 
void connectToNetwork() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Establishing connection to WiFi..");
  }
  Serial.println("Connected to network");
}

// SPIFFS Functions

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
  Serial.printf("Listing directory: %s\r\n", dirname);

  File root = fs.open(dirname);
  if(!root){
    Serial.println("- failed to open directory");
    return;
  }
  if(!root.isDirectory()){
    Serial.println(" - not a directory");
    return;
  }

  File file = root.openNextFile();
  while(file){
    if(file.isDirectory()){
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if(levels){
        listDir(fs, file.name(), levels -1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("\tSIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\r\n", path);

    File file = fs.open(path);
    if(!file || file.isDirectory()){
        Serial.println("- failed to open file for reading");
        return;
    }

    Serial.println("- read from file:");
    while(file.available()){
        Serial.write(file.read());
    }
    file.close();
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\r\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("- failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("- file written");
    } else {
        Serial.println("- write failed");
    }
    file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Appending to file: %s\r\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("- failed to open file for appending");
        return;
    }
    if(file.print(message)){
        Serial.println("- message appended");
    } else {
        Serial.println("- append failed");
    }
    file.close();
}

void renameFile(fs::FS &fs, const char * path1, const char * path2){
    Serial.printf("Renaming file %s to %s\r\n", path1, path2);
    if (fs.rename(path1, path2)) {
        Serial.println("- file renamed");
    } else {
        Serial.println("- rename failed");
    }
}

void deleteFile(fs::FS &fs, const char * path){
    Serial.printf("Deleting file: %s\r\n", path);
    if(fs.remove(path)){
        Serial.println("- file deleted");
    } else {
        Serial.println("- delete failed");
    }
}

// Config Functions

void loadConfiguration(const char *filename, Config &config) {
  File file = SPIFFS.open(filename);
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.println(F("Failed to read file, using default configuration"));
  }
  config.matrix_brightness = doc["matrix_brightness"] | default_matrix_brightness;
  // strlcpy(config.hostname,                  // <- destination
  //         doc["hostname"] | "example.com",  // <- source
  //         sizeof(config.hostname));         // <- destination's capacity
  file.close();
}

void saveConfiguration(const char *filename, const Config &config) {
  SPIFFS.remove(filename);
  File file = SPIFFS.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println(F("Failed to create file"));
    return;
  }
  StaticJsonDocument<512> doc;
  doc["matrix_brightness"] = config.matrix_brightness;
  if (serializeJson(doc, file) == 0) {
    Serial.println(F("Failed to write to file"));
  }
  file.close();
}

// MQTT Functions

void store_payload(const char* payload) {
  strcpy(msg_payload, payload);
  msg_payload_changed = true;
}

void parse_mqtt_payload(const char* payload) {
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.print("Parsing MQTT payload failed: ");
    Serial.println(error.f_str());
    return;
  }
  // you can do doc["time"].as<long>();
  if (doc.containsKey("msg")) store_payload(doc["msg"]);
  if (doc.containsKey("brightness")) config.matrix_brightness = doc["brightness"];

  Serial.println(F("Saving configuration..."));
  saveConfiguration(config_filename, config);
}

void mqtt_callback(const char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
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
    Serial.print("Attempting MQTT connection...");
    if (mqtt_client.connect("arduinoClient")) {
      Serial.println("MQTT connected.");
      // mqtt_client.publish(announce_topic, announce_msg);
      mqtt_client.subscribe(control_topic);
      Serial.print("Subscribed MQTT topic: ");
      Serial.println(control_topic);
    } else {
      Serial.print("MQTT connection failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" - try again in 5 seconds");
      delay(5000);
    }
  }
}

// Matrix Funtions

void draw_centered_text(Adafruit_GFX* canvas, const char* text, int x, 
    bool center_x, int y, bool center_y, 
    int size, int r, int g, int b, const GFXfont* font) {
  int16_t  x1, y1, xa, ya;
  uint16_t w, h;

  canvas->setFont(font);
  canvas->setTextSize(size);
  canvas->setTextColor(dma_display.color565(r, g, b));
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
  overlay_canvas.fillScreen(dma_display.color444(0, 0, 0));

  // MQTT status
  if (!mqtt_client.connected()) {
    overlay_canvas.drawCircle(overlay_canvas.width() - 2, 2, 2, 
      dma_display.color565(255, 0, 0));
  }

  // Payload - TODO 
  if (strlen(msg_payload) > 0) {
    draw_centered_text(&overlay_canvas, msg_payload, -1, true, -1, true, 
      1, 255, 106, 0, &FreeSansBold9pt7b);
    msg_payload_changed = false;
  }

  // Time and date
  time_t t = time_client.getEpochTime();
  char buf[50];
  sprintf(buf, "%04d/%02d/%02d", year(t), month(t), day(t));
  draw_centered_text(&overlay_canvas, buf, -1, true, 1, false, 1, 255, 0, 0, NULL);
  sprintf(buf, "%02d:%02d", hour(t), minute(t));
  draw_centered_text(&overlay_canvas, buf, -1, true, -1, false, 
    1, 255, 238, 0, &FreeSansBold9pt7b);
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
          dma_display.drawPixelRGB565(x + xOffset, y, usTemp[xOffset]);
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
      dma_display.drawPixelRGB565(x, y, usPalette[*s++]);
    }
  }
  // Apply the overlay - TODO: Should be done inside the above loops
  for (x = 0; x < pDraw->iWidth; x++) {
    uint16_t c = overlay_canvas.getPixel(x, y);
    if (c > 0) {
      dma_display.drawPixel(x, y, c);
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
  if (gif.open(name, gif_open_file, gif_close_file, gif_read_file, gif_seek_file, gif_draw)) {
    x_offset = (MATRIX_WIDTH - gif.getCanvasWidth())/2;
    if (x_offset < 0) x_offset = 0;
    y_offset = (MATRIX_HEIGHT - gif.getCanvasHeight())/2;
    if (y_offset < 0) y_offset = 0;
    Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
    Serial.flush();
    gif_loaded = true;
  } else {
    Serial.printf("Failed to open GIF: %s\n", name);
  }
}

// Arduino Functions

void setup() {
  // Generic setup
  Serial.begin(115200);
 
  // SPIFFS setup
  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
      Serial.println("SPIFFS Mount Failed");
      return;
  }
  listDir(SPIFFS, "/", 0);  
  // SPIFFS.format(); // should go into MQTT handler

  // Load configuration
  Serial.println(F("Loading configuration..."));
  loadConfiguration(config_filename, config);

  // WiFi setup
  scanNetworks();
  connectToNetwork();
 
  Serial.println(WiFi.macAddress());
  Serial.print("Acquired IP address: ");
  Serial.println(WiFi.localIP());

  // NTP setup
  time_client.begin();
  time_client.setTimeOffset(3600);

  // MQTT setup
  mqtt_client.setClient(wifi_client);
  mqtt_client.setServer(mqtt_server, 1883);
  mqtt_client.setCallback(mqtt_callback);

  // Matrix setup
  dma_display.setPanelBrightness(config.matrix_brightness);
  dma_display.begin();
  dma_display.fillScreen(dma_display.color444(0, 0, 0));

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
    Serial.println("NTP updating time...");
    if (ntp_last_update == 0) {
      draw_centered_text(&dma_display, "NTP", -1, true, -1, true, 
        1, 0, 0, 255, NULL);
    }
    while (!time_client.update()) {
      Serial.println("NTP forcing update...");
      time_client.forceUpdate();
    }
    if (ntp_last_update == 0) {
        dma_display.clearScreen();
    }
    current_secs = time_client.getEpochTime();
    ntp_last_update = current_secs;
    setTime(current_secs);
    Serial.print("NTP client updated, current epoch: ");
    Serial.println(current_secs);
  }

  // MQTT loop
  if (!mqtt_client.connected()) {
    reconnect();
  }
  mqtt_client.loop();

  // Matrix loop
  if (msg_payload_changed || matrix_last_update == 0 ||
      current_secs - matrix_last_update > 1) {
    draw_overlay();
    msg_payload_changed = false;
    matrix_last_update = current_secs;
  }

  // AnimatedGIF loop
  int delayMillisecs;
  if (gif_loaded) {
    if (gif.playFrame(false, &delayMillisecs) < 0) {
      Serial.println("Failed to play GIF frame.");
    }
  } else {
    load_gif("/gifs/Alien2.gif");
  }

  dma_display.setPanelBrightness(config.matrix_brightness);
  Serial.printf("Sleeping for (msecs): %d\n", delayMillisecs);
  if (delayMillisecs > 0) {
    delay(delayMillisecs); // sleep till the end of the frame
  }
  // TODO: better loop here and call playFrame in time?
}