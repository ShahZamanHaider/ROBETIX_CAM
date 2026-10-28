#pragma once
#include "AppOptions.h"
#include <esp_camera.h>
#include <ArduinoJson.h>
namespace Motion {void begin();bool available();uint32_t score();bool testing();void showAreas(bool enabled=true);void calibrate(bool enabled=true);void reset();void rebase();bool analyze(camera_fb_t*,const AppOptions&);bool active(uint32_t tailMs);void status(JsonObject);}
