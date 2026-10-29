#include "Settings.h"
#include <esp_system.h>
#include <mbedtls/sha256.h>
#include <ArduinoJson.h>
#include "FactoryPolicy.h"
#include <WiFi.h>
Preferences prefs;
String adminHash, authSalt, setupSsid, setupPassword, hardwareTag;
bool cloudCleanupPending=false;
bool factoryLabelNumeric=false;
static String newFactoryPassword(){uint32_t value;do{value=esp_random();}while(!FactoryPolicy::randomAccepted(value));char digits[9];snprintf(digits,sizeof(digits),"%08lu",(unsigned long)FactoryPolicy::digits(value));return String(digits);}
String randomHex(size_t bytes) {
  String out;if(!out.reserve(bytes*2))return "";
  for(size_t i=0;i<bytes;i++) { char b[3]; snprintf(b,sizeof(b),"%02x",uint8_t(esp_random())); if(!out.concat(b))return ""; }
  return out;
}
static String saltedHash(const String &salt,const String &password) {
 uint8_t digest[32];mbedtls_sha256_context context;mbedtls_sha256_init(&context);
 int result=mbedtls_sha256_starts_ret(&context,0);if(!result)result=mbedtls_sha256_update_ret(&context,(const uint8_t*)salt.c_str(),salt.length());if(!result)result=mbedtls_sha256_update_ret(&context,(const uint8_t*)password.c_str(),password.length());if(!result)result=mbedtls_sha256_finish_ret(&context,digest);mbedtls_sha256_free(&context);if(result)return "";
 char output[65];for(unsigned i=0;i<32;i++)snprintf(output+i*2,3,"%02x",digest[i]);return String(output);
}

String passwordHash(const String &password){return saltedHash(authSalt,password);}
bool setAdminPassword(const String &password){
 if(password.length()<12||password.length()>64)return false;
 String salt=randomHex(16),hash=saltedHash(salt,password);if(salt.length()!=32||hash.length()!=64)return false;StaticJsonDocument<256>d;d["salt"]=salt;d["hash"]=hash;String saved;serializeJson(d,saved);
 if(d.overflowed()||!prefs.putString("authState",saved)||prefs.getString("authState","")!=saved)return false;
 authSalt=salt;adminHash=hash;return true;
}
void settingsBegin() {
  Preferences factory;factory.begin("rv_factory",false);
  setupSsid=factory.getString("ssid","");setupPassword=factory.getString("password","");hardwareTag=factory.getString("hardware","");
  if(setupPassword.isEmpty()||hardwareTag.isEmpty()||setupSsid.isEmpty()){
    bool occupied[100]={};WiFi.persistent(false);WiFi.mode(WIFI_STA);int count=WiFi.scanNetworks(false,true,false,120);
    for(int i=0;i<count;i++){String name=WiFi.SSID(i);for(unsigned n=1;n<=99;n++){char expected[24];snprintf(expected,sizeof(expected),"ROBETIX-VISION-%02u",n);if(name==expected)occupied[n]=true;}}
    WiFi.scanDelete();unsigned number=FactoryPolicy::number(ESP.getEfuseMac(),occupied);
    if(!number){Serial.println("No free factory Wi-Fi name nearby; installer provisioning required");delay(5000);ESP.restart();}
    char name[24];snprintf(name,sizeof(name),"ROBETIX-VISION-%02u",number);setupSsid=name;
    setupPassword=newFactoryPassword();hardwareTag=randomHex(16);
    bool ok=setupPassword.length()==8&&hardwareTag.length()==32&&factory.putString("ssid",setupSsid)&&factory.putString("password",setupPassword)&&factory.putString("hardware",hardwareTag);
    if(!ok){Serial.println("Factory provisioning failed; reboot before use");delay(1000);ESP.restart();}

  }
  factoryLabelNumeric=FactoryPolicy::numeric8(setupPassword.c_str());
  cloudCleanupPending=factory.getString("pendingOwner","").length()>0;bool finishReset=factory.getBool("resetPending",false);factory.end();
  prefs.begin("robetix",false);
  // Complete a reset before any workers start, including after power loss or
  // an old in-flight worker writing during the final restart interval.
  if(finishReset){if(!prefs.clear()){Serial.println("Reset cleanup failed; retrying after restart");delay(1000);ESP.restart();}Preferences done;if(!done.begin("rv_factory",false)||!done.remove("resetPending")){Serial.println("Reset completion could not be saved");delay(1000);ESP.restart();}done.end();}
  adminHash=prefs.getString("admin","");
  if(!prefs.isKey("setupDone"))prefs.putBool("setupDone",!adminHash.isEmpty());
  // Preserve existing administrator and Wi-Fi settings. Factory card survives normal resets.
  if(!prefs.isKey("netReady")) prefs.putBool("netReady",!adminHash.isEmpty());
  authSalt=prefs.getString("salt","");
  if(authSalt.isEmpty()) { authSalt=randomHex(16);if(authSalt.length()!=32||!prefs.putString("salt",authSalt)){Serial.println("Authentication storage unavailable");delay(1000);ESP.restart();} }
  if(prefs.isKey("authState")){StaticJsonDocument<256>d;String saved=prefs.getString("authState","");if(!deserializeJson(d,saved)&&d["salt"].is<const char*>()&&strlen(d["salt"])==32&&d["hash"].is<const char*>()&&strlen(d["hash"])==64){authSalt=d["salt"].as<String>();adminHash=d["hash"].as<String>();}else adminHash="physical-recovery-required";}
}

bool factoryResetSettings(bool credentialsRevoked){
 // Keep factory label/identity outside the resettable namespace. Never erase
 // remote ownership: only the signed-in cloud owner can release that claim.
 Preferences factory;if(!factory.begin("rv_factory",false))return false;
 DynamicJsonDocument cloud(6144);String saved=prefs.getString("cloud","");
 if(saved.length()&&!deserializeJson(cloud,saved)){
  String id=cloud["id"]|"";
  if(id.length()&&(!factory.putString("pendingOwner",id)||factory.getString("pendingOwner","")!=id)){factory.end();return false;}
 }
 if(!factory.putBool("pendingRevoke",!credentialsRevoked)){factory.end();return false;}
 if(!factory.putBool("resetPending",true)){factory.end();return false;}
 factory.end();return prefs.clear();
}


// Called only by the nonce-confirmed physical USB maintenance command. Current
// network settings must already be persisted, so issuing a new factory label
// never changes active Wi-Fi or cloud identity, including across reboot.
bool provisionFactoryLabel(){
 if(!prefs.isKey("network"))return false;
 StaticJsonDocument<1024> network;if(deserializeJson(network,prefs.getString("network",""))||!network["ap_ssid"].is<const char*>()||!network["ap_password"].is<const char*>())return false;
 Preferences factory;if(!factory.begin("rv_factory",false))return false;
 String next=newFactoryPassword();bool ok=next.length()==8&&factory.putString("password",next)>0&&factory.getString("password","")==next;
 if(ok){setupPassword=next;factoryLabelNumeric=true;factory.putUChar("wifiPolicy",2);}factory.end();return ok;
}
