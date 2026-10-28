#pragma once
#include <Arduino.h>
#include <esp_camera.h>
#include <ArduinoJson.h>
#include <atomic>
namespace Camera {
extern SemaphoreHandle_t mutex;
extern std::atomic<bool> ready;
extern uint16_t pid;
extern String error;
extern uint32_t captured, failures, captureUs;
bool begin();
bool pause(bool enabled);
bool extras(int whiteBalance,int exposure,int gain,bool persist);
camera_fb_t* capture(); // Caller owns mutex until esp_camera_fb_return.
bool configure(int size,int quality,int brightness,int contrast,int saturation,int mirror,int flip,bool persist);
void status(JsonObject out);
void benchmarkTask(void*);
bool startBenchmark();
void benchmarkStatus(JsonObject out);
bool busy();
}

