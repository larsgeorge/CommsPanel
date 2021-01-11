#pragma once

#include <Arduino.h>
#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <ArduinoJson.h>
#include <PubSubClient.h>

#include "Debug.h"
#include "Config.h"

class MQTTTask {
  private:
    IPAddress* _mqtt_server;
    const char* TOPIC_ANNOUNCE = "comms_panel_announce";
    const char* TOPIC_CONTROL = "comms_panel_control";

    PubSubClient* _mqtt_client;
    char _message[40];
    bool _messsage_changed = false;
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

    void store_payload(const char* msg);
    void parse_mqtt_payload(const char* payload);
    void mqtt_callback(const char* topic, byte* payload, unsigned int length);
    void reconnect();
    
  public:
    MQTTTask();

    /**
     * Starts the underlying task with the default values.
     * Optionally performs an update right away.
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

    /**
     * Set to true when MQTT update is active, false otherwise.
     */
    bool is_updating();

    bool connected();
};
