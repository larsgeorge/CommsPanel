#include "MQTTTask.h"

MQTTTask::MQTTTask() {
  _mqtt_client = new PubSubClient();
}

bool MQTTTask::_handleOnce(void* pvParameter) {
  _updating = true;
  if (!_mqtt_client->connected()) {
    reconnect();
  }
  _mqtt_client->loop();
  _updating = false;
  return true;
}

void MQTTTask::_handle(void* pvParameter) {
  for (;;) {
    _handleOnce(pvParameter);
     vTaskDelay(1 * 1000 / portTICK_PERIOD_MS); // 1secs
  }
}

void MQTTTask::begin() {
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
  _mqtt_server = new IPAddress(192, 168, 1, 220);
    
#if defined(ESP32_RTOS) && defined(ESP32)
  // if enabled, start RTOS task now
  Debug.println("Creating RTOS MQTT task...");
  _task_create_result = xTaskCreate(
    _handle,                 /* Task function */
    "MQTT_TASK_HANDLE",      /* String with name of task */
    10000,                   /* Stack size in bytes */
    (void*) this,            /* Parameter passed as input of the task */
    1,                       /* Priority of the task */
    &_task_handle);          /* Task handle */
#endif
  _task_setup = true;
}

bool MQTTTask::update() {
  if (!_task_setup) begin();
#ifndef ESP32_RTOS
  return _handleOnce((void*) this); 
#endif  
}

void MQTTTask::end() {
  if (_task_setup) {
    // _mqtt_client->?
    if (_task_create_result == pdPASS) {
      vTaskDelete(_task_handle);
      _task_create_result = pdFALSE;
    }
    _task_setup = false;
  }
}

bool MQTTTask::is_updating() {
  return this->_updating;
}

void MQTTTask::store_payload(const char* msg) {
  strcpy(_message, msg);
  _messsage_changed = true;
}

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

void MQTTTask::parse_mqtt_payload(const char* payload) {
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Debug.print(F("Parsing MQTT payload failed: "));
    Debug.println(error.f_str());
    return;
  }
  Config.update(doc);
}

void MQTTTask::mqtt_callback(const char* topic, byte* payload, unsigned int length) {
  Debug.print(F("Message arrived ["));
  Debug.print(topic);
  Debug.print(F("] "));
  for (int i = 0; i < length; i++) {
    Debug.print((char) payload[i]);
  }
  Debug.println();
  char* pl = (char*) malloc(length * sizeof(char) + 1);
  memcpy(pl, payload, length);
  pl[length] = 0;
  parse_mqtt_payload(pl);
  free(pl);
}

void MQTTTask::reconnect() {
  while (!_mqtt_client->connected()) {
    Debug.print(F("Attempting MQTT connection..."));
    if (_mqtt_client->connect("arduinoClient")) {
      Debug.println(F("MQTT connected."));
      // _mqtt_client.publish(announce_topic, announce_msg);
      _mqtt_client->subscribe(TOPIC_CONTROL);
      Debug.print(F("Subscribed MQTT topic: "));
      Debug.println(TOPIC_CONTROL);
    } else {
      Debug.print(F("MQTT connection failed, rc="));
      Debug.print(_mqtt_client->state());
      Debug.println(F(" - try again in 5 seconds"));
      delay(5000);
    }
  }
}

bool MQTTTask::connected() {
  return _mqtt_client->connected();
}


