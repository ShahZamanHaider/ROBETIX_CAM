#pragma once
#include <Arduino.h>
#include <atomic>
#include <ArduinoJson.h>
namespace Network { void begin(); void tick(); void changed(); bool configure(JsonObject,String&,bool&);void status(JsonObject);bool connected();extern std::atomic<uint32_t> reconnects; }
