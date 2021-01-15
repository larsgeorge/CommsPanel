#pragma once

#include <Arduino.h>
#include <Time.h>
#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <AnimatedGIF.h>

#include "Debug.h"
#include "Config.h"

class MatrixTask {
  private:
    static MatrixPanel_I2S_DMA* _matrix;
    static GFXcanvas16* _overlay_canvas; 
    unsigned long _matrix_last_update = 0;
    char _buffer[80];
    bool _mqtt_connected = false;
    bool _ntp_updating = false;

    // AnimatedGIF
    static AnimatedGIF* _gif;
    static File _gif_file;
    unsigned long _start_tick = 0;
    bool _gif_loaded = false;
    bool _screen_dirty = true;
    time_t _time_epoch;

    // Task
    bool _task_setup = false;
    BaseType_t _task_create_result = pdFALSE;
    TaskHandle_t _task_handle = NULL;
    
    /**
     * Private handler function that does the update logic.
     * Might be called in an RTOS task or via update() in the
     * main loop() function.
     * 
     * @return true on success, false otherwise
     */
    void _handle(void * pvParameter);
    bool _handleOnce(void * pvParameter, uint32_t time_increment_ms);
    
    // Matrix functions
    void draw_centered_text(Adafruit_GFX* canvas, const char* text, int x, 
      bool center_x, int y, bool center_y, int size, uint16_t rgb,
      const GFXfont* font);
    void draw_overlay();

    // AnimatedGIF functions
    static void gif_draw(GIFDRAW *pDraw);
    static void * gif_open_file(const char *fname, int32_t *pSize);
    static void gif_close_file(void *pHandle);
    static int32_t gif_read_file(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen);
    static int32_t gif_seek_file(GIFFILE *pFile, int32_t iPosition);
    void show_gif_loop(const char* name);
    void load_gif(const char* name);

  public:
    MatrixTask();

    /**
     * Starts the underlying task with the default values.
     */
    void begin();

    /**
     * This should be called in the main loop of your application, 
     * unless the RTOS option is enabled (in which case calling 
     * update() is a no-op).
     *
     * @return true on success, false on failure
     */
    bool update();

    /**
     * Stops the underlying task.
     */
    void end();

    void info(const char* text);
    void clear();
    void set_mqtt_connected(bool mqtt_connected);
    void set_ntp_updating(bool ntp_updating);
    void set_time_epoch(time_t time_epoch);
};
