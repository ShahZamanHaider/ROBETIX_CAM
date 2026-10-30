#pragma once
#include "AppOptions.h"
#include <Arduino.h>
#include <ArduinoJson.h>
namespace Engine {
enum class Action:uint8_t{Start,Stop,Photo,Privacy,Camera,Options,Benchmark,Restart,Light,ResetCamera,FactoryReset};
struct Command{Action action;bool flag=false;int camera[10]={};AppOptions options;uint64_t mask=UINT64_MAX;char remoteId[33]={};};
bool begin();bool submit(const Command&);bool configure(JsonObject,String&,const char* remoteId=nullptr);
bool occupied();bool paused();void status(JsonObject);void options(JsonObject);String cameraName();
size_t frameCapacity();void requestPreview();bool copyFrame(uint8_t*,size_t,size_t&,uint32_t&,uint32_t&,uint32_t after=0);
bool setTime(uint32_t);uint32_t epoch();
}
