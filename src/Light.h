#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
namespace Light {void begin();void set(uint8_t percent);void tick();uint8_t value();bool configure(uint8_t preferred,uint16_t seconds);void status(JsonObject);}
