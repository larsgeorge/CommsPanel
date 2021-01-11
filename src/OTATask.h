#pragma once

#include <Arduino.h>
#ifdef ESP32
#include <WiFi.h>
#include <ESPmDNS.h>
#else
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#endif

#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include "Debug.h"

class OTATask {
  private:
    const char* _nameprefix;
    const char* _ssid;
    const char* _password;
    bool _updating = false;

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
    bool _handleOnce(void * pvParameter);
    
  public:
    OTATask();
    OTATask(const char* nameprefix, const char* ssid, const char* password);

    /**
     * Starts the underlying task with the default values.
     * By default enables the WiFi station, but can be overridden 
     * optionally as necessary. 
     */
    void begin();
    void begin(bool enable_wifi);

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

    /**
     * Set to true when OTA update is active, false otherwise.
     */
    bool is_updating();
};
