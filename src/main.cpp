#include <Arduino.h>
#include <Time.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "FS.h"
#include "SPIFFS.h"

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <AnimatedGIF.h>

#define ESP32_RTOS
#define DEBUG_ENABLE_Debug
#define DEBUG_ENABLE_TELNET
#include "Debug.h"
#include "Config.h"
#include "Common.h"
#include "Storage.h"
#include "OTATask.h"
#include "NTPTask.h"
#include "MQTTTask.h"

#include <Credentials.h>

char buffer[80];
const char* DEVICE_HOSTNAME = "commspanel";

NTPTask ntpTask;
OTATask otaTask(DEVICE_HOSTNAME, WLAN_IOT_SSID, WLAN_IOT_PASSWORD);
MQTTTask mqttTask;

// Matrix
const uint16_t matrix_width = 64;
const uint16_t matrix_height = 64;

MatrixPanel_I2S_DMA matrix;
GFXcanvas16 overlay_canvas(matrix_width, matrix_height); 
unsigned long matrix_last_update = 0;

// AnimatedGIF
AnimatedGIF gif;
File gif_file;
unsigned long start_tick = 0;
bool gif_loaded = false;
bool screen_dirty = true;

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
  // Debug.print("x1: "); Debug.print(x1);
  // Debug.print(", y1: "); Debug.print(y1);
  // Debug.print(", w: "); Debug.print(w);
  // Debug.print(", h: "); Debug.println(h);
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
  // Debug.print("xa: "); Debug.print(xa);
  // Debug.print(", ya: "); Debug.println(ya);
  canvas->setCursor(xa, ya); 
  canvas->print(text);
}

void draw_overlay() {
  // Clear screen and set some text defaults
  overlay_canvas.setTextSize(1);     // size 1 == 8 pixels high
  overlay_canvas.setTextWrap(false);
  overlay_canvas.fillScreen(matrix.color444(0, 0, 0));

  // // MQTT status
  if (!mqttTask.connected()) {
    overlay_canvas.drawCircle(overlay_canvas.width() - 2, 2, 2, 
      matrix.color565(255, 0, 0));
  }

  // // Payload - TODO 
  // if (strlen(message) > 0) {
  //   draw_centered_text(&overlay_canvas, message, -1, true, -1, true, 
  //     1, config.color_message, &FreeSansBold9pt7b);
  //   messsage_changed = false;
  // }

  // Time and date
  if (Config.data().show_time || Config.data().show_date) {
    time_t t = ntpTask.get_time_epoch();
    if (Config.data().show_date) {
      sprintf(buffer, "%04d/%02d/%02d", year(t), month(t), day(t));
      draw_centered_text(&overlay_canvas, buffer, -1, true, 1, false,
        1, Config.data().color_date, NULL);
    }
    if (Config.data().show_time) {
      sprintf(buffer, "%02d:%02d", hour(t), minute(t));
      draw_centered_text(&overlay_canvas, buffer, -1, true, -1, false, 
        1, Config.data().color_time, &FreeSansBold9pt7b);
    }
  }
}

void info(const char* text) {
  if (!Config.data().screen_off) {
    draw_centered_text(&matrix, text, -1, true, -1, true, 
      1, matrix.color565(0, 0, 255), NULL);
  }
}

void clear() {
  if (!Config.data().screen_off) {
    matrix.clearScreen();
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
//  Debug.printf("Seek time = %d us\n", i);
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
    Debug.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
    Debug.flush();
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
  sprintf(buffer, "%s/%s.gif", Config.GIF_PATH, name);
  if (gif.open(buffer, gif_open_file, gif_close_file, gif_read_file, gif_seek_file, gif_draw)) {
    x_offset = (MATRIX_WIDTH - gif.getCanvasWidth())/2;
    if (x_offset < 0) x_offset = 0;
    y_offset = (MATRIX_HEIGHT - gif.getCanvasHeight())/2;
    if (y_offset < 0) y_offset = 0;
    Debug.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
    Debug.flush();
    gif_loaded = true;
  } else {
    Debug.printf("Failed to open GIF: %s\n", buffer);
  }
}

// Arduino Functions

void setup() {
  // Generic setup
  Debug.begin(115200);

  // WiFi setup
  Common.scanNetworks();
  // Common.connectToNetwork(DEVICE_HOSTNAME); // done in otaTask.begin() 
 
  // OTA setup
  info("OTA");
  otaTask.begin(true);
  clear();

  Storage.begin();
  Config.begin();
 
  info("NTP");
  ntpTask.begin(true);
  clear();

  info("MQTT");
  mqttTask.begin();
  clear();

  // Matrix setup
  matrix.setPanelBrightness(Config.data().brightness);
  matrix.begin();
  matrix.fillScreen(matrix.color444(0, 0, 0));

  // AnimatedGIF setup  
  gif.begin(LITTLE_ENDIAN_PIXELS);

  // Allow the hardware to sort itself out
  delay(1500);
}

void loop() {
  // current_secs = time_client.getEpochTime();
  otaTask.update();
  ntpTask.update();
  mqttTask.update();

  matrix.setPanelBrightness(Config.data().brightness);
  if (!Config.data().screen_off) {
    screen_dirty = true;
    // Matrix loop
    // if (messsage_changed || matrix_last_update == 0 ||
    //     current_secs - matrix_last_update > 1) {
    //   draw_overlay();
    //   messsage_changed = false;
    //   matrix_last_update = current_secs;
    // }

    // AnimatedGIF loop
    int delayMillisecs = 0;
    if (gif_loaded) {
      if (gif.playFrame(false, &delayMillisecs) < 0) {
        Debug.println(F("Failed to play GIF frame."));
      }
    } else {
      load_gif(Config.data().gif_name);
    }
    //Debug.printf("Sleeping for (msecs): %d\n", delayMillisecs);
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