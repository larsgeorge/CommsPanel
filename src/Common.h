#ifndef _COMMON_H_
#define _COMMON_H_

#include <Arduino.h>
#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif

#include "Debug.h"

class CommonClass {
  private:
    String translateEncryptionType(wifi_auth_mode_t encryptionType);

  public:
    CommonClass();
    void begin();  

    void scanNetworks();
    void connectToNetwork(const char* hostname, const char* ssid, const char* password);
};

extern CommonClass Common;

#endif /* _COMMON_H_ */