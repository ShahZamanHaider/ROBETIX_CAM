#include <Arduino.h>
#include "Settings.h"
#include "Camera.h"
#include "Network.h"
#include "Storage.h"
#include "Web.h"
#include "Engine.h"
#include "Light.h"
void setup() {
  Serial.begin(115200); delay(500);
  Serial.println("\nROBETIX VISION / Phase 2 / standalone OV2640");
  settingsBegin(); Camera::begin(); Storage::begin(); Network::begin();if(!Engine::begin())Serial.println("Capture service failed to start");webBegin();
  Serial.printf("Heap=%u minimum=%u PSRAM=%u free=%u\n",ESP.getFreeHeap(),ESP.getMinFreeHeap(),ESP.getPsramSize(),ESP.getFreePsram());
}
void loop() { Network::tick();Light::tick();delay(25); }
