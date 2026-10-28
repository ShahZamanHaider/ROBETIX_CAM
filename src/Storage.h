#pragma once
#include <Arduino.h>
#include <esp_camera.h>
#include <ArduinoJson.h>
#include <SD_MMC.h>
#include <atomic>
namespace Storage {
extern std::atomic<bool> available;
extern SemaphoreHandle_t operationMutex;
void begin();void status(JsonObject);void refresh();void fail(const char*);
bool reserve(uint32_t bytes,uint16_t reserveMB,bool cleanup);
String newName(const char*extension,uint32_t epoch);
bool save(camera_fb_t*,String&,uint32_t epoch=0);
bool validName(const String&,const String&kind);
String path(const String&name,const String&kind);
bool list(const String&kind,uint32_t before,JsonObject out,uint32_t from=0,uint32_t to=UINT32_MAX);
bool remove(const String&name,const String&kind);
uint32_t sequence(const String&name);
}
