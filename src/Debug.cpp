#include "Debug.h"

DebugClass::DebugClass() {
}

DebugClass::DebugClass(bool enable_serial, bool enable_telnet) {
  this->_serial_enabled = enable_serial;
  this->_telnet_enable = enable_telnet;
}

void DebugClass::begin() {
#ifdef DEBUG_ENABLE_SERIAL
  bool serial = true;
#else
  bool serial = this->_serial_enabled;
#endif
#ifdef DEBUG_ENABLE_TELNET
  bool telnet = true;
#else
  bool telnet = this->_telnet_enable;
#endif  
  this->begin(serial, telnet, this->DEFAULT_SERIAL_BAUD);
}

void DebugClass::begin(bool enable_serial, bool enable_telnet) {
  this->begin(enable_serial, enable_telnet, this->DEFAULT_SERIAL_BAUD);
}

void DebugClass::begin(uint64_t baud) {
#ifdef DEBUG_ENABLE_SERIAL
  bool serial = true;
#else
  bool serial = this->_serial_enabled;
#endif
#ifdef DEBUG_ENABLE_TELNET
  bool telnet = true;
#else
  bool telnet = this->_telnet_enable;
#endif  
  this->begin(serial, telnet, baud);
}

void DebugClass::begin(bool enable_serial, bool enable_telnet, uint64_t baud) {
  this->_serial_enabled = enable_serial;
  this->_telnet_enable = enable_telnet;
  if (this->_serial_enabled) Serial.begin(baud);
  if (this->_telnet_enable) TelnetStream.begin();
}

void DebugClass::end() {
  if (this->_serial_enabled) Serial.end();
  if (this->_telnet_enable) TelnetStream.stop();
}

int DebugClass::read() {
  int res = -1;
  if (this->_serial_enabled) res = Serial.read();
  if (this->_telnet_enable) {
    if (res > -1) {
      res += TelnetStream.read();
    } else {
      res = TelnetStream.read();
    }
  }
  return res;
}

int DebugClass::available() {
  int res = 0;
  if (this->_serial_enabled) res += Serial.available();
  if (this->_telnet_enable) res += TelnetStream.available();
  return res;
}

int DebugClass::peek() {
  int res = -1;
  if (this->_serial_enabled) res = Serial.peek();
  else if (this->_telnet_enable) res = TelnetStream.read();
  return res;
}

size_t DebugClass::write(uint8_t val) {
  size_t res1 = 0, res2 = 0;
  if (this->_serial_enabled) res1 = Serial.write(val);
  if (this->_telnet_enable) res2 = TelnetStream.write(val);
  if (res1 > 0) return res1;
  else return res2;
}

size_t DebugClass::write(const uint8_t *buf, size_t size) {
  size_t res1 = 0, res2 = 0;
  if (this->_serial_enabled) res1 = Serial.write(buf, size);
  if (this->_telnet_enable) res2 = TelnetStream.write(buf, size);
  if (res1 > 0) return res1;
  else return res2;
}

void DebugClass::flush() {
  if (this->_serial_enabled) Serial.flush();
  if (this->_telnet_enable) TelnetStream.flush();
}

DebugClass Debug;
