#include "Settings.h"
#include <esp_system.h>
#include <mbedtls/sha256.h>
#include <ArduinoJson.h>
Preferences prefs;
String adminHash, authSalt, setupSsid, setupPassword, hardwareTag;
String randomHex(size_t bytes) {
  String out; out.reserve(bytes * 2);
  for(size_t i=0;i<bytes;i++) { char b[3]; snprintf(b,sizeof(b),"%02x",uint8_t(esp_random())); out += b; }
  return out;
}
static String saltedHash(const String &salt,const String &password) {
  String input=salt+password; uint8_t digest[32];
  mbedtls_sha256_ret((const unsigned char*)input.c_str(),input.length(),digest,0);
  String out; for(auto b:digest) { char h[3]; snprintf(h,3,"%02x",b); out+=h; } return out;
}
String passwordHash(const String &password){return saltedHash(authSalt,password);}
bool setAdminPassword(const String &password){
 if(password.length()<12||password.length()>64)return false;
 String salt=randomHex(16),hash=saltedHash(salt,password);StaticJsonDocument<256>d;d["salt"]=salt;d["hash"]=hash;String saved;serializeJson(d,saved);
 if(d.overflowed()||!prefs.putString("authState",saved)||prefs.getString("authState","")!=saved)return false;
 authSalt=salt;adminHash=hash;return true;
}
void settingsBegin() {
  Preferences factory;factory.begin("rv_factory",false);
  setupSsid=factory.getString("ssid","");setupPassword=factory.getString("password","");hardwareTag=factory.getString("hardware","");
  if(setupPassword.isEmpty()||hardwareTag.isEmpty()){
    setupSsid="ROBETIX-"+randomHex(3);setupPassword=randomHex(10);hardwareTag=randomHex(16);
    bool ok=factory.putString("ssid",setupSsid)&&factory.putString("password",setupPassword)&&factory.putString("hardware",hardwareTag);
    if(!ok){Serial.println("Factory provisioning failed; reboot before use");delay(1000);ESP.restart();}

  }factory.end();
  prefs.begin("robetix",false);
  adminHash=prefs.getString("admin","");
  if(!prefs.isKey("setupDone"))prefs.putBool("setupDone",!adminHash.isEmpty());
  // Preserve existing administrator and Wi-Fi settings. Factory card survives normal resets.
  if(!prefs.isKey("netReady")) prefs.putBool("netReady",!adminHash.isEmpty());
  authSalt=prefs.getString("salt","");
  if(authSalt.isEmpty()) { authSalt=randomHex(16); prefs.putString("salt",authSalt); }
  if(prefs.isKey("authState")){StaticJsonDocument<256>d;String saved=prefs.getString("authState","");if(!deserializeJson(d,saved)&&d["salt"].is<const char*>()&&strlen(d["salt"])==32&&d["hash"].is<const char*>()&&strlen(d["hash"])==64){authSalt=d["salt"].as<String>();adminHash=d["hash"].as<String>();}else adminHash="physical-recovery-required";}
}

