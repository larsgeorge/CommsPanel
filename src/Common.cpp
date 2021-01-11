#include "Common.h"

CommonClass::CommonClass() {
}

void CommonClass::begin() {
}

void CommonClass::begin(const char* hostname) {
  _hostname = hostname;
}

String CommonClass::translateEncryptionType(wifi_auth_mode_t encryptionType) {
  switch (encryptionType) {
    case (WIFI_AUTH_OPEN):
      return "Open";
    case (WIFI_AUTH_WEP):
      return "WEP";
    case (WIFI_AUTH_WPA_PSK):
      return "WPA_PSK";
    case (WIFI_AUTH_WPA2_PSK):
      return "WPA2_PSK";
    case (WIFI_AUTH_WPA_WPA2_PSK):
      return "WPA_WPA2_PSK";
    case (WIFI_AUTH_WPA2_ENTERPRISE):
      return "WPA2_ENTERPRISE";
    case (WIFI_AUTH_MAX):
      return "MAX";
  }
  return "UNKNOWN";
}
 
void CommonClass::scanNetworks() {
  int numberOfNetworks = WiFi.scanNetworks();
  Debug.print(F("Number of networks found: "));
  Debug.println(numberOfNetworks);
  for (int i = 0; i < numberOfNetworks; i++) {
    Debug.print(F("Network name: "));
    Debug.println(WiFi.SSID(i));
    Debug.print(F("Signal strength: "));
    Debug.println(WiFi.RSSI(i));
    Debug.print(F("MAC address: "));
    Debug.println(WiFi.BSSIDstr(i));
    Debug.print(F("Encryption type: "));
    String encryptionTypeDescription = translateEncryptionType(WiFi.encryptionType(i));
    Debug.println(encryptionTypeDescription);
    Debug.println(F("-----------------------"));
  }
}
 
void CommonClass::connectToNetwork(const char* hostname) {
  WiFi.setHostname(hostname);
  WiFi.begin(WLAN_IOT_SSID, WLAN_IOT_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Debug.println(F("Establishing connection to WiFi.."));
  }
  Debug.println(F("Connected to network"));
  Debug.print(F("MAC address: "));
  Debug.println(WiFi.macAddress());
  Debug.print(F("Acquired IP address: "));
  Debug.println(WiFi.localIP());
}

CommonClass Common;

