#pragma once
#include "AppOptions.h"
#include <esp_camera.h>
#include <ArduinoJson.h>
namespace Motion {void reset();void rebase();bool analyze(camera_fb_t*,const AppOptions&);bool active(uint32_t tailMs);void status(JsonObject);}
