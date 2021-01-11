#ifndef _STORAGE_H_
#define _STORAGE_H_

#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>

#define FORMAT_SPIFFS_IF_FAILED true

#include "Debug.h"

class StorageClass {
  private:
    fs::FS* _fs;

  public:
    StorageClass();
    void begin();
    void begin(fs::FS &fs);

    fs::FS* get_file_system();
    void list_dir(const char* dirname, uint8_t levels);
    char* read_file(const char* path);
    void dump_file(const char* path);
    void write_file(const char* path, const char* message);
    void write_binary_file(const char* path, const uint8_t* data, size_t size);
    void append_file(const char* path, const char * message);
    void rename_file(const char* path1, const char* path2);
    void delete_file(const char* path);
};

extern StorageClass Storage;

#endif /* _STORAGE_H_ */