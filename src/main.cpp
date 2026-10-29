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
#include "ServiceOrigin.h"
#include <WiFi.h>
static void networkTest(){StaticJsonDocument<512>d;d["connected"]=Network::connected();d["gateway"]=WiFi.gatewayIP().toString();d["dns"]=WiFi.dnsIP().toString();if(Network::connected()){WiFiClient probe;d["gateway_tcp"]=probe.connect(WiFi.gatewayIP(),80,1500);probe.stop();d["internet_tcp"]=probe.connect(IPAddress(1,1,1,1),443,1500);probe.stop();String host=ServiceOrigin::url;host.remove(0,8);IPAddress address;bool resolved=WiFi.hostByName(host.c_str(),address)==1;d["service_dns_ok"]=resolved;d["service_ip"]=address.toString();if(resolved){d["service_tcp"]=probe.connect(address,443,1500);probe.stop();}}Serial.print("ROBETIX_NETWORK_TEST:");serializeJson(d,Serial);Serial.println();}
void setup() {
  Serial.begin(115200); delay(500);
  Serial.println("\nROBETIX VISION / Phase 2 / standalone OV2640");
  settingsBegin(); Camera::begin(); Storage::begin(); Network::begin();if(!Engine::begin())Serial.println("Capture service failed to start");webBegin();Serial.println("ROBETIX_RECOVERY_READY");
  Serial.printf("Heap=%u minimum=%u PSRAM=%u free=%u\n",ESP.getFreeHeap(),ESP.getMinFreeHeap(),ESP.getPsramSize(),ESP.getFreePsram());
}
void loop() { static String resetNonce;static uint32_t resetDeadline=0;static char command[100];static uint8_t used=0;while(Serial.available()){char ch=Serial.read();if(ch=='\n'){command[used]=0;if(!strcmp(command,"RECOVERY_HELLO")){resetNonce=randomHex(16);resetDeadline=millis()+30000;Serial.printf("ROBETIX_USB_CONFIRM:%s\n",resetNonce.c_str());}else if(!strncmp(command,"FACTORY_RESET:",14)){if(resetNonce.length()&&int32_t(resetDeadline-millis())>0&&resetNonce==String(command+14)){resetNonce="";Engine::Command reset;reset.action=Engine::Action::FactoryReset;Serial.println(Engine::submit(reset)?"ROBETIX_USB_RESET:accepted":"ROBETIX_USB_RESET:busy");}else Serial.println("ROBETIX_USB_RESET:expired");}else if(!strcmp(command,"ADMIN_RECOVERY")||!strcmp(command,"TRUST_DEVICE")){String code=Auth::armRecovery();Serial.printf("ROBETIX_RECOVERY_CODE:%s (one use, 5 minutes; do not save or share)\n",code.c_str());}else if(!strcmp(command,"NETWORK_TEST")){networkTest();}else if(!strcmp(command,"DIAGNOSTICS")){DynamicJsonDocument status(7168);DeviceStatus::write(status.to<JsonObject>());Serial.print("ROBETIX_DIAGNOSTICS:");serializeJson(status,Serial);Serial.println();}else if(!strcmp(command,"SETUP_CARD"))Serial.printf("ROBETIX_SETUP_CARD:{\"ssid\":\"%s\",\"password\":\"%s\",\"url\":\"http://192.168.4.1\"}\n",setupSsid.c_str(),setupPassword.c_str());used=0;}else if(ch!='\r'){if(used<sizeof(command)-1)command[used++]=ch;else used=0;}}Network::tick();Light::tick();delay(25); }
