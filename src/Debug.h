#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <Arduino.h>
#ifdef ESP32
#else
#endif

#include "NetTypes.h"
#include <TelnetStream.h>

class DebugClass : public Stream {
  private:
    const uint64_t DEFAULT_SERIAL_BAUD = 115200;

    bool _serial_enabled = true;
    bool _telnet_enable = false;
    uint64_t baud = DEFAULT_SERIAL_BAUD;

  public:
    DebugClass();
    DebugClass(bool enable_serial, bool enable_telnet);

    /**
     * Starts the underlying output streams.
     */
    void begin();
    void begin(bool enable_serial, bool enable_telnet);
    void begin(uint64_t baud);
    void begin(bool enable_serial, bool enable_telnet, uint64_t baud);

    /**
     * Stops the underlying output streams.
     */
    void end();
      
    // Stream implementation
    int read();
    int available();
    int peek();

    // Print implementation
    virtual size_t write(uint8_t val);
    virtual size_t write(const uint8_t *buf, size_t size);
    using Print::write; // pull in write(str) and write(buf, size) from Print
    virtual void flush();
};

extern DebugClass Debug;

#endif /* _DEBUG_H_ */