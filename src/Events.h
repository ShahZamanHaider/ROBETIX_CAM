#pragma once
#include <ArduinoJson.h>
#include <Arduino.h>
namespace Events {
void begin();uint32_t add(uint32_t epoch,const String&photo,const String&video,const char*note,bool cardOwned,const char*camera="Camera",uint8_t score=0);
void delivery(uint32_t id,const char*state);void attach(uint32_t id,const String&video,bool cardOwned);void complete(const String&video,bool ok,bool cardOwned);
void list(JsonObject);uint32_t latest();
}
