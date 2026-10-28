#pragma once
#include "AppOptions.h"
#include <ArduinoJson.h>
#include <Arduino.h>
namespace Maintenance {
void begin();void policy(const AppOptions&);
bool find(const String&kind,uint32_t from,uint32_t to,String&error);
bool confirm(const String&token,String&error);void status(JsonObject);
}
