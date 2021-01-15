#include <Arduino.h>

#define ESP32_RTOS
#define DEBUG_ENABLE_Debug
#define DEBUG_ENABLE_TELNET
#include <Credentials.h>
#include "Debug.h"
#include "Config.h"
#include "Common.h"
#include "Storage.h"
#include "OTATask.h"
#include "NTPTask.h"
#include "MQTTTask.h"
#include "MatrixTask.h"

char buffer[80];
const char* DEVICE_HOSTNAME = "commspanel";

NTPTask ntpTask;
OTATask otaTask(DEVICE_HOSTNAME, WLAN_IOT_SSID, WLAN_IOT_PASSWORD);
MQTTTask mqttTask;
MatrixTask matrixTask;

// Arduino Functions

void setup() {
  Debug.begin(115200);
  
  Common.scanNetworks();
  // Common.connectToNetwork(DEVICE_HOSTNAME); // done in otaTask.begin() 
  
  matrixTask.info("OTA");
  otaTask.begin(true);
  matrixTask.clear();
  
  Storage.begin();
  Config.begin();

  matrixTask.info("NTP");
  ntpTask.begin(true);
  matrixTask.clear();

  matrixTask.info("MQTT");
  mqttTask.begin();
  matrixTask.clear();

  matrixTask.begin();
}

void loop() {
  otaTask.update();
  ntpTask.update();
  mqttTask.update();
  matrixTask.set_time_epoch(ntpTask.get_time_epoch());
  matrixTask.update();
}