#include "Storage.h"

StorageClass::StorageClass() {
}

void StorageClass::begin() {
  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
    Debug.println(F("SPIFFS Mount Failed"));
    return;
  }
  _fs = &SPIFFS; 
  list_dir("/", 0);  
  // SPIFFS.format(); // should go into MQTT handler
}

void StorageClass::begin(fs::FS &fs) {
  _fs = &fs;
  list_dir("/", 0); 
}

fs::FS* StorageClass::get_file_system() {
  return _fs;
}

void StorageClass::list_dir(const char* dirname, uint8_t levels) {
  Debug.printf("Listing directory: %s\r\n", dirname);
  File root = _fs->open(dirname);
  if(!root){
    Debug.println(F("- failed to open directory"));
    return;
  }
  if(!root.isDirectory()){
    Debug.println(F(" - not a directory"));
    return;
  }
  File file = root.openNextFile();
  while(file){
    if(file.isDirectory()){
      Debug.print(F("  DIR : "));
      Debug.println(file.name());
      if(levels){
        list_dir(file.name(), levels -1);
      }
    } else {
      Debug.print(F("  FILE: "));
      Debug.print(file.name());
      Debug.print(F("\tSIZE: "));
      Debug.println(file.size());
    }
    file = root.openNextFile();
  }
}

char* StorageClass::read_file(const char* path) {
  Debug.printf("Reading file: %s\n", path);
  char* buf;
  File file = _fs->open(path);
  if (!file || file.isDirectory()){
    Debug.println(F("- failed to open file for reading"));
    return NULL;
  }
  unsigned int size = file.size();
  buf = (char*) malloc(size + 1);
  file.readBytes(buf, size);
  buf[size] = '\0';
  //Debug.println(buf);
  file.close();
  return buf;
}

void StorageClass::dump_file(const char* path) {
  Debug.printf("Reading file: %s\n", path);
  File file = _fs->open(path);
  if (!file || file.isDirectory()){
    Debug.println(F("- failed to open file for reading"));
    return;
  }
  Debug.println(F("- read from file:"));
  while(file.available()) {
    Debug.write(file.read());
  }
  file.close();
}

void StorageClass::write_file(const char* path, const char* message) {
  Debug.printf("Writing file: %s\n", path);
  File file = _fs->open(path, FILE_WRITE);
  if (!file){
    Debug.println(F("- failed to open file for writing"));
    return;
  }
  if (file.print(message)){
    Debug.println(F("- file written"));
  } else {
    Debug.println(F("- write failed"));
  }
  file.close();
}

void StorageClass::write_binary_file(const char* path, const uint8_t* data, size_t size) {
  Debug.printf("Writing binary file: %s\n", path);
  File file = _fs->open(path, FILE_WRITE);
  if (!file){
    Debug.println(F("- failed to open file for writing"));
    return;
  }
  if (file.write(data, size)){
    Debug.println(F("- file written"));
  } else {
    Debug.println(F("- write failed"));
  }
  file.close();
}

void StorageClass::append_file(const char* path, const char * message) {
  Debug.printf("Appending to file: %s\n", path);
  File file = _fs->open(path, FILE_APPEND);
  if (!file){
    Debug.println(F("- failed to open file for appending"));
    return;
  }
  if (file.print(message)){
    Debug.println(F("- message appended"));
  } else {
    Debug.println(F("- append failed"));
  }
  file.close();
}

void StorageClass::rename_file(const char* path1, const char* path2) {
  Debug.printf("Renaming file %s to %s\n", path1, path2);
  if (_fs->rename(path1, path2)) {
    Debug.println(F("- file renamed"));
  } else {
    Debug.println(F("- rename failed"));
  }
}

void StorageClass::delete_file(const char* path) {
  Debug.printf("Deleting file: %s\n", path);
  if(_fs->remove(path)){
    Debug.println(F("- file deleted"));
  } else {
    Debug.println(F("- delete failed"));
  }
}

StorageClass Storage;
