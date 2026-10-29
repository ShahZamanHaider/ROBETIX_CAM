#include <Arduino.h>
#include "Settings.h"
#include "Camera.h"
#include "Network.h"
#include "Storage.h"
#include "Web.h"
#include "Engine.h"
#include "Light.h"
#include "Auth.h"
#include "DeviceStatus.h"
void setup() {
  Serial.begin(115200); delay(500);
  Serial.println("\nROBETIX VISION / Phase 2 / standalone OV2640");
  settingsBegin(); Camera::begin(); Storage::begin(); Network::begin();if(!Engine::begin())Serial.println("Capture service failed to start");webBegin();
  Serial.printf("Heap=%u minimum=%u PSRAM=%u free=%u\n",ESP.getFreeHeap(),ESP.getMinFreeHeap(),ESP.getPsramSize(),ESP.getFreePsram());
}
void loop() { static char command[20];static uint8_t used=0;while(Serial.available()){char ch=Serial.read();if(ch=='\n'){command[used]=0;if(!strcmp(command,"ADMIN_RECOVERY")||!strcmp(command,"TRUST_DEVICE")){String code=Auth::armRecovery();Serial.printf("ROBETIX_RECOVERY_CODE:%s (one use, 5 minutes; do not save or share)\n",code.c_str());}else if(!strcmp(command,"DIAGNOSTICS")){DynamicJsonDocument status(7168);DeviceStatus::write(status.to<JsonObject>());Serial.print("ROBETIX_DIAGNOSTICS:");serializeJson(status,Serial);Serial.println();}else if(!strcmp(command,"SETUP_CARD"))Serial.printf("ROBETIX_SETUP_CARD:{\"ssid\":\"%s\",\"password\":\"%s\",\"url\":\"http://192.168.4.1\"}\n",setupSsid.c_str(),setupPassword.c_str());used=0;}else if(ch!='\r'){if(used<sizeof(command)-1)command[used++]=ch;else used=0;}}Network::tick();Light::tick();delay(25); }
