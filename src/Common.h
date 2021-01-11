#ifndef _COMMON_H_
#define _COMMON_H_

#include <Arduino.h>
#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif

#include "Debug.h"
#include <Credentials.h>

class CommonClass {
  private:
    const char* _hostname;

    String translateEncryptionType(wifi_auth_mode_t encryptionType);

  public:
    CommonClass();
    void begin();  
    void begin(const char* hostname);  

    void scanNetworks();
    void connectToNetwork(const char* hostname);
};

extern CommonClass Common;

#endif /* _COMMON_H_ */