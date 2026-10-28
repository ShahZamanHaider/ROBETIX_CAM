#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_camera.h>
namespace Live {
void begin(); void close(); void ticket(JsonVariantConst); void tick();
bool wanted(); bool viewing(); uint8_t profile(); void offer(camera_fb_t*,bool busy); void status(JsonObject);
}
