#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_camera.h>
namespace Cloud {void nameChanged();void halt();void begin();bool configure(JsonObject,String&);void status(JsonObject);bool test(String&);bool wantsTest();void offer(camera_fb_t*,uint32_t event,uint32_t epoch,const char*camera,uint8_t score,int offset);}
