#pragma once

#include <Arduino.h>
#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <NTPClient.h>

#include "Debug.h"

class NTPTask {
  private:
    const uint32_t DEFAULT_UPDATE_INTERVAL_SECS = 3600; // 1h
    const uint32_t DEFAULT_TIME_OFFSET = 3600; 

    NTPClient* _time_client;
    uint64_t _ntp_last_update = 0;
    uint64_t _current_secs = 0;
    uint32_t _update_interval = DEFAULT_UPDATE_INTERVAL_SECS;
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
    
    void init_client();

  public:
    NTPTask();
    NTPTask(uint32_t update_interval);

    /**
     * Starts the underlying task with the default values.
     * Optionally performs an update right away.
     */
    void begin();
    void begin(bool update);

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
     * Set the update interval to another frequency. E.g. useful when the
     * timeOffset should not be set in the constructor.
     */
    void set_update_interval(unsigned int update_interval);

    /**
     * Set to true when NTP update is active, false otherwise.
     */
    bool is_updating();

    /**
     * Returns the current time as epoch.
     */
    uint64_t get_time_epoch();
};
