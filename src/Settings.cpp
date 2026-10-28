#include "Settings.h"
#include <esp_system.h>
#include <mbedtls/sha256.h>
Preferences prefs;
String adminHash, authSalt, setupSsid, setupPassword, hardwareTag;
String randomHex(size_t bytes) {
  String out; out.reserve(bytes * 2);
  for(size_t i=0;i<bytes;i++) { char b[3]; snprintf(b,sizeof(b),"%02x",uint8_t(esp_random())); out += b; }
  return out;
}
String passwordHash(const String &password) {
  String input=authSalt+password; uint8_t digest[32];
  mbedtls_sha256_ret((const unsigned char*)input.c_str(),input.length(),digest,0);
  String out; for(auto b:digest) { char h[3]; snprintf(h,3,"%02x",b); out+=h; } return out;
}
void settingsBegin() {
  Preferences factory;factory.begin("rv_factory",false);
  setupSsid=factory.getString("ssid","");setupPassword=factory.getString("password","");hardwareTag=factory.getString("hardware","");
  if(setupPassword.isEmpty()||hardwareTag.isEmpty()){
    setupSsid="ROBETIX-"+randomHex(3);setupPassword=randomHex(10);hardwareTag=randomHex(16);
    bool ok=factory.putString("ssid",setupSsid)&&factory.putString("password",setupPassword)&&factory.putString("hardware",hardwareTag);
    if(!ok){Serial.println("Factory provisioning failed; reboot before use");delay(1000);ESP.restart();}
    Serial.printf("ROBETIX_SETUP_CARD:{\"ssid\":\"%s\",\"password\":\"%s\",\"url\":\"http://192.168.4.1\"}\n",setupSsid.c_str(),setupPassword.c_str());
  }factory.end();
  prefs.begin("robetix",false);
  adminHash=prefs.getString("admin","");
  // Preserve existing administrator and Wi-Fi settings. Factory card survives normal resets.
  if(!prefs.isKey("netReady")) prefs.putBool("netReady",!adminHash.isEmpty());
  authSalt=prefs.getString("salt","");
  if(authSalt.isEmpty()) { authSalt=randomHex(16); prefs.putString("salt",authSalt); }
}

