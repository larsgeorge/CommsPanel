#include "MatrixTask.h"

File MatrixTask::_gif_file;
MatrixPanel_I2S_DMA* MatrixTask::_matrix;
GFXcanvas16* MatrixTask::_overlay_canvas; 
AnimatedGIF* MatrixTask::_gif;

MatrixTask::MatrixTask() {
  _matrix = new MatrixPanel_I2S_DMA();
  _overlay_canvas = new GFXcanvas16(MATRIX_WIDTH, MATRIX_HEIGHT);
  _gif = new AnimatedGIF();
}

bool MatrixTask::_handleOnce(void* pvParameter, uint32_t time_increment_ms) {
  //MatrixTask* task = (MatrixTask *) pvParameter;
  time_increment_ms = 0;
  _matrix->setPanelBrightness(Config.data().brightness);
  if (!Config.data().screen_off) {
    _screen_dirty = true;
    // Matrix loop
    // if (messsage_changed || matrix_last_update == 0 ||
    //     current_secs - matrix_last_update > 1) {
    //   draw_overlay();
    //   messsage_changed = false;
    //   matrix_last_update = current_secs;
    // }

    // AnimatedGIF loop
    if (Config.data().show_gif) {
      if (_gif_loaded) {
        int delay;
        if (_gif->playFrame(false, &delay) < 0) {
          Debug.println(F("Failed to play GIF frame."));
        }
        time_increment_ms = (uint32_t) delay;
      } else {
        load_gif(Config.data().gif_name);
      }
    }
  } else { // screen is off
    if (_screen_dirty) {
      _matrix->clearScreen();
      _screen_dirty = false;
    } else {
      _matrix->drawPixelRGB565(0, 0, _matrix->color565(0, 255, 0));
      _screen_dirty = true;
    }
    time_increment_ms = 1000; // blink dot per second
  }  
  return true;
}

void MatrixTask::_handle(void* pvParameter) {
  TickType_t last_wake_time, time_increment_ms;
  last_wake_time = xTaskGetTickCount();
  for (;;) {
    _handleOnce(pvParameter, (uint32_t) &time_increment_ms);
    vTaskDelayUntil(&last_wake_time, time_increment_ms / portTICK_PERIOD_MS);
  }
}

void MatrixTask::begin() {
#ifdef DEBUG_ENABLE_SERIAL
  bool serial = true;
#else
  bool serial = false;
#endif
#ifdef DEBUG_ENABLE_TELNET
  bool telnet = true;
#else
  bool telnet = false;
#endif
  Debug.begin(serial, telnet);
  _gif->begin(LITTLE_ENDIAN_PIXELS);
  _matrix->setPanelBrightness(Config.data().brightness);
  _matrix->begin();
  _matrix->fillScreen(_matrix->color444(0, 0, 0));
#if defined(ESP32_RTOS) && defined(ESP32)
  // if enabled, start RTOS task now
  Debug.println("Creating RTOS Matrix task...");
  _task_create_result = xTaskCreate(
    _handle,                 /* Task function */
    "MATRIX_TASK_HANDLE",    /* String with name of task */
    10000,                   /* Stack size in bytes */
    (void*) this,            /* Parameter passed as input of the task */
    1,                       /* Priority of the task */
    &_task_handle);          /* Task handle */
#endif
  _task_setup = true;
}

bool MatrixTask::update() {
  if (!_task_setup) begin();
#ifndef ESP32_RTOS
  return _handleOnce((void*) this, 1000); // TODO - Fix this non-RTOS handling! 
#endif  
}

void MatrixTask::end() {
  if (_task_setup) {
    if (_task_create_result == pdPASS) {
      vTaskDelete(_task_handle);
      _task_create_result = pdFALSE;
    }
    _task_setup = false;
  }
}

void MatrixTask::set_mqtt_connected(bool mqtt_connected) {
  _mqtt_connected = mqtt_connected;
}

void MatrixTask::set_ntp_updating(bool ntp_updating) {
  _ntp_updating = ntp_updating;
}

void MatrixTask::set_time_epoch(time_t time_epoch) {
  _time_epoch = time_epoch;
}

// Matrix Funtions

void MatrixTask::draw_centered_text(Adafruit_GFX* canvas, const char* text, int x, 
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

void MatrixTask::draw_overlay() {
  // Clear screen and set some text defaults
  _overlay_canvas->setTextSize(1);     // size 1 == 8 pixels high
  _overlay_canvas->setTextWrap(false);
  _overlay_canvas->fillScreen(_matrix->color444(0, 0, 0));

  // // MQTT status
  if (!_mqtt_connected) {
    _overlay_canvas->drawCircle(_overlay_canvas->width() - 2, 2, 2, 
      _matrix->color565(255, 0, 0));
  }

  // // Payload - TODO 
  // if (strlen(message) > 0) {
  //   draw_centered_text(&overlay_canvas, message, -1, true, -1, true, 
  //     1, config.color_message, &FreeSansBold9pt7b);
  //   messsage_changed = false;
  // }

  // Time and date
  if (Config.data().show_time || Config.data().show_date) {
    time_t t = _time_epoch;
    if (Config.data().show_date) {
      sprintf(_buffer, "%04d/%02d/%02d", year(t), month(t), day(t));
      draw_centered_text(_overlay_canvas, _buffer, -1, true, 1, false,
        1, Config.data().color_date, NULL);
    }
    if (Config.data().show_time) {
      sprintf(_buffer, "%02d:%02d", hour(t), minute(t));
      draw_centered_text(_overlay_canvas, _buffer, -1, true, -1, false, 
        1, Config.data().color_time, &FreeSansBold9pt7b);
    }
  }
}

void MatrixTask::info(const char* text) {
  if (!Config.data().screen_off) {
    draw_centered_text(_matrix, text, -1, true, -1, true, 
      1, _matrix->color565(0, 0, 255), NULL);
  }
}

void MatrixTask::clear() {
  if (!Config.data().screen_off) {
    _matrix->clearScreen();
  }
}

// AnimatedGIF Functions

// Draw a line of image directly on the LED Matrix
void MatrixTask::gif_draw(GIFDRAW *pDraw) {
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
          _matrix->drawPixelRGB565(x + xOffset, y, usTemp[xOffset]);
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
      _matrix->drawPixelRGB565(x, y, usPalette[*s++]);
    }
  }
  // Apply the overlay - TODO: Should be done inside the above loops
  for (x = 0; x < pDraw->iWidth; x++) {
    uint16_t c = _overlay_canvas->getPixel(x, y);
    if (c > 0) {
      _matrix->drawPixel(x, y, c);
    }
  }
}

void* MatrixTask::gif_open_file(const char *fname, int32_t *pSize) {
  _gif_file = SPIFFS.open(fname);
  if (_gif_file) {
    *pSize = _gif_file.size();
    return (void *)&_gif_file;
  }
  return NULL;
}

void MatrixTask::gif_close_file(void *pHandle) {
  File *file = static_cast<File *>(pHandle);
  if (file != NULL)
     file->close();
}

int32_t MatrixTask::gif_read_file(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
    int32_t iBytesRead;
    iBytesRead = iLen;
    File *file = static_cast<File *>(pFile->fHandle);
    // Note: If you read a file all the way to the last byte, seek() stops working
    if ((pFile->iSize - pFile->iPos) < iLen)
       iBytesRead = pFile->iSize - pFile->iPos - 1; // <-- ugly work-around
    if (iBytesRead <= 0)
       return 0;
    iBytesRead = (int32_t)file->read(pBuf, iBytesRead);
    pFile->iPos = file->position();
    return iBytesRead;
}

int32_t MatrixTask::gif_seek_file(GIFFILE *pFile, int32_t iPosition) { 
  int i = micros();
  File *file = static_cast<File *>(pFile->fHandle);
  file->seek(iPosition);
  pFile->iPos = (int32_t)file->position();
  i = micros() - i;
//  Debug.printf("Seek time = %d us\n", i);
  return pFile->iPos;
}

void MatrixTask::show_gif_loop(const char* name) {
  _start_tick = millis();
  int x_offset, y_offset;
   
  if (_gif->open(name, gif_open_file, gif_close_file, gif_read_file, gif_seek_file, gif_draw))   {
    x_offset = (MATRIX_WIDTH - _gif->getCanvasWidth())/2;
    if (x_offset < 0) x_offset = 0;
    y_offset = (MATRIX_HEIGHT - _gif->getCanvasHeight())/2;
    if (y_offset < 0) y_offset = 0;
    Debug.printf("Successfully opened GIF; Canvas size = %d x %d\n", _gif->getCanvasWidth(), _gif->getCanvasHeight());
    Debug.flush();
    while (_gif->playFrame(true, NULL))     {      
      if ( (millis() - _start_tick) > 8000) { // we'll get bored after about 8 seconds of the same looping gif
        break;
      }
    }
    _gif->close();
  }
}

void MatrixTask::load_gif(const char* name) {
  int x_offset, y_offset;
  sprintf(_buffer, "%s/%s.gif", Config.GIF_PATH, name);
  if (_gif->open(_buffer, gif_open_file, gif_close_file, gif_read_file, gif_seek_file, gif_draw)) {
    x_offset = (MATRIX_WIDTH - _gif->getCanvasWidth())/2;
    if (x_offset < 0) x_offset = 0;
    y_offset = (MATRIX_HEIGHT - _gif->getCanvasHeight())/2;
    if (y_offset < 0) y_offset = 0;
    Debug.printf("Successfully opened GIF; Canvas size = %d x %d\n", _gif->getCanvasWidth(), _gif->getCanvasHeight());
    Debug.flush();
    _gif_loaded = true;
  } else {
    Debug.printf("Failed to open GIF: %s\n", _buffer);
  }
}