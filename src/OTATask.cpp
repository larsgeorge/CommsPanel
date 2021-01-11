#include "OTATask.h"

OTATask::OTATask() {
}

OTATask::OTATask(const char* nameprefix, const char* ssid, const char* password) {
  _nameprefix = nameprefix;
  _ssid = ssid;
  _password = password;
}

bool OTATask::_handleOnce(void* pvParameter) {
  // OTATask* task = (OTATask *) pvParameter;
  _updating = true;
  ArduinoOTA.handle();
  _updating = false;
  return true;
}

void OTATask::_handle(void* pvParameter) {
  for (;;) {
    _handleOnce(pvParameter);
     vTaskDelay(1 * 1000 / portTICK_PERIOD_MS); // 1sec
  }
}

void OTATask::begin() {
  begin(true);
}

void OTATask::begin(bool enable_wifi) {
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
  
  // Configure the hostname
  uint16_t maxlen = strlen(_nameprefix) + 7;
  char *fullhostname = new char[maxlen];
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(fullhostname, maxlen, "%s-%02x%02x%02x", _nameprefix, mac[3], mac[4], mac[5]);
  ArduinoOTA.setHostname(fullhostname);
  delete[] fullhostname;

  // Configure and start the WiFi station
  if (enable_wifi) {
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(_nameprefix);
    WiFi.begin(_ssid, _password);
    // Wait for connection
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Debug.println("Connection Failed! Rebooting...");
      delay(5000);
      ESP.restart();
    }
  }

  // Port defaults to 3232, set via:
  //   ArduinoOTA.setPort(3232); 
  // No authentication by default, set via:
  //   ArduinoOTA.setPassword("admin");
  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Debug.println("OTA: Start updating " + type);
  });
  
  ArduinoOTA.onEnd([]() {
    Debug.println("\nOTA: End updating");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Debug.printf("OTA: Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Debug.printf("OTA: Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Debug.println("\nAuth Failed");
    else if (error == OTA_BEGIN_ERROR) Debug.println("\nBegin Failed");
    else if (error == OTA_CONNECT_ERROR) Debug.println("\nConnect Failed");
    else if (error == OTA_RECEIVE_ERROR) Debug.println("\nReceive Failed");
    else if (error == OTA_END_ERROR) Debug.println("\nEnd Failed");
  });

  ArduinoOTA.begin();

  Debug.println("OTA Initialized!");
  Debug.print("IP address: ");
  Debug.println(WiFi.localIP());

#if defined(ESP32_RTOS) && defined(ESP32)
  // if enabled, start RTOS task now
  Debug.println("Creating RTOS OTA task...");
  _task_create_result = xTaskCreate(
    _handle,           /* Task function */
    "OTA_TASK_HANDLE",       /* String with name of task */
    10000,                   /* Stack size in bytes */
    (void*) this,            /* Parameter passed as input of the task */
    1,                       /* Priority of the task */
    &_task_handle);    /* Task handle */
#endif
  _task_setup = true;
}

bool OTATask::update() {
  if (!_task_setup) begin();
#ifndef ESP32_RTOS
  return _handleOnce((void*) this); 
#endif  
}

void OTATask::end() {
  if (_task_setup) {
    if (_task_create_result == pdPASS) {
      vTaskDelete(_task_handle);
      _task_create_result = pdFALSE;
    }
    _task_setup = false;
  }
}

bool OTATask::is_updating() {
  return _updating;
}
