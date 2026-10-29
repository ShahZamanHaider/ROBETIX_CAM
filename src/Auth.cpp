#include "Auth.h"
#include "Settings.h"
#include "RecoveryWindow.h"
#include "LocalAccessWindow.h"
#include <esp_timer.h>
namespace Auth {
struct Device{char hash[65]={};bool remember=false;int64_t since=0,adminUntil=0;};
static constexpr unsigned deviceLimit=8;static Device devices[deviceLimit];static SemaphoreHandle_t mutex;static unsigned next=0;
static char recoveryHash[65]={};static RecoveryWindow recoveryWindow;
static char handoffHash[65]={},handoffParent[65]={},handoffToken[65]={};static LocalAccessWindow handoffWindow;
void begin(){mutex=xSemaphoreCreateMutex();if(!mutex)return;for(int i=0;i<deviceLimit;i++){String h=prefs.getString((String("device")+i).c_str(),"");if(h.length()==64){strlcpy(devices[i].hash,h.c_str(),65);devices[i].remember=true;}}}
static bool equal(const char*a,const char*b){if(!a||!b||strlen(a)!=64||strlen(b)!=64)return false;uint8_t diff=0;for(int i=0;i<64;i++)diff|=a[i]^b[i];return diff==0;}
static String issueLocked(bool remember){
 String token=randomHex(32);if(token.length()!=64)return "";String hash=passwordHash(token);if(hash.length()!=64)return "";
 unsigned slot=next++%deviceLimit;for(unsigned i=0;i<deviceLimit;i++)if(!devices[i].hash[0]){slot=i;break;}
 String key=String("device")+slot;
 if(remember&&!prefs.putString(key.c_str(),hash))return "";
 if(!remember)prefs.remove(key.c_str());strlcpy(devices[slot].hash,hash.c_str(),65);devices[slot].remember=remember;devices[slot].since=esp_timer_get_time();devices[slot].adminUntil=0;return token;
}
String issue(bool remember){if(!mutex)return "";xSemaphoreTake(mutex,portMAX_DELAY);String token=issueLocked(remember);xSemaphoreGive(mutex);return token;}
String handoff(const String&token){if(!mutex)return "";xSemaphoreTake(mutex,portMAX_DELAY);String hash=passwordHash(token),code;for(auto&d:devices)if(d.hash[0]&&equal(d.hash,hash.c_str())&&(d.remember||esp_timer_get_time()-d.since<12LL*60*60*1000000)){code=randomHex(16);strlcpy(handoffHash,passwordHash(code).c_str(),65);strlcpy(handoffParent,d.hash,65);strlcpy(handoffToken,token.c_str(),65);handoffWindow.start(esp_timer_get_time());break;}xSemaphoreGive(mutex);return code;}
String acceptHandoff(const String&code,bool&remember){if(!mutex||code.length()!=32)return "";xSemaphoreTake(mutex,portMAX_DELAY);String token;if(handoffHash[0]&&handoffWindow.permit(esp_timer_get_time())&&equal(handoffHash,passwordHash(code).c_str())){for(auto&d:devices)if(d.hash[0]&&equal(d.hash,handoffParent)&&(d.remember||esp_timer_get_time()-d.since<12LL*60*60*1000000)){remember=d.remember;token=handoffToken;break;}if(token.length()){handoffHash[0]=0;memset(handoffToken,0,sizeof(handoffToken));handoffWindow.finish();}}xSemaphoreGive(mutex);return token;}
static bool physical(const String&code,String&error){if(code.length()!=32||!recoveryHash[0]||!recoveryWindow.permit(esp_timer_get_time())||!equal(recoveryHash,passwordHash(code).c_str())){error="Recovery code is invalid or expired. Request a new code using USB.";return false;}return true;}
String trust(const String&code,String&error){if(!mutex)return "";xSemaphoreTake(mutex,portMAX_DELAY);String token;if(physical(code,error)){token=issueLocked(true);if(token.length()){recoveryWindow.finish();recoveryHash[0]=0;}else error="Could not create session";}xSemaphoreGive(mutex);return token;}
bool authorize(const String&token,const String&proof,String&error){if(!mutex)return false;xSemaphoreTake(mutex,portMAX_DELAY);String hash=passwordHash(token);bool ok=false;for(auto&d:devices)if(d.hash[0]&&equal(d.hash,hash.c_str())){bool password=!prefs.getBool("cardOwner",false)&&proof.length()>=12&&passwordHash(proof)==adminHash;if(password||physical(proof,error)){d.adminUntil=esp_timer_get_time()+300000000LL;ok=true;if(!password){recoveryWindow.finish();recoveryHash[0]=0;}}break;}xSemaphoreGive(mutex);return ok;}
bool sensitive(const String&token){if(!mutex)return false;xSemaphoreTake(mutex,portMAX_DELAY);String hash=passwordHash(token);bool ok=false;for(auto&d:devices)if(d.hash[0]&&equal(d.hash,hash.c_str())){ok=d.adminUntil>esp_timer_get_time();break;}xSemaphoreGive(mutex);return ok;}
bool valid(const String&token){
 if(!mutex||token.length()!=64)return false;bool ok=false;xSemaphoreTake(mutex,portMAX_DELAY);String hash=passwordHash(token);
 for(auto&d:devices)if(d.hash[0]&&equal(d.hash,hash.c_str())){ok=d.remember||esp_timer_get_time()-d.since<12LL*60*60*1000000;if(!ok)d.hash[0]=0;break;}
 xSemaphoreGive(mutex);return ok;
}
void revoke(const String&token){if(!mutex||token.length()!=64)return;xSemaphoreTake(mutex,portMAX_DELAY);String hash=passwordHash(token);for(int i=0;i<deviceLimit;i++)if(devices[i].hash[0]&&equal(devices[i].hash,hash.c_str())){devices[i]=Device{};prefs.remove((String("device")+i).c_str());break;}xSemaphoreGive(mutex);}
String armRecovery(){if(!mutex)return "";String code=randomHex(16);xSemaphoreTake(mutex,portMAX_DELAY);strlcpy(recoveryHash,passwordHash(code).c_str(),sizeof(recoveryHash));recoveryWindow.start(esp_timer_get_time());xSemaphoreGive(mutex);return code;}
bool recover(const String&code,const String&password,String&error){if(!mutex)return false;xSemaphoreTake(mutex,portMAX_DELAY);bool ok=false;
 if(code.length()!=32||!recoveryHash[0]||!recoveryWindow.permit(esp_timer_get_time())){error="Recovery code is invalid or expired. Request a new code using USB.";}
 else{if(!equal(recoveryHash,passwordHash(code).c_str()))error="Recovery code is invalid or expired. Request a new code using USB.";else if(password.length()<12||password.length()>64)error="Choose a 12–64 character administrator password";else if(!setAdminPassword(password))error="Settings write failed";else{prefs.putBool("cardOwner",false);recoveryHash[0]=0;recoveryWindow.finish();for(int i=0;i<deviceLimit;i++){devices[i]=Device{};prefs.remove((String("device")+i).c_str());}ok=true;}}
 xSemaphoreGive(mutex);return ok;
}
}

namespace Auth {void tick(){if(!mutex||xSemaphoreTake(mutex,0)!=pdTRUE)return;if(handoffToken[0]&&(!handoffWindow.active||esp_timer_get_time()>=handoffWindow.until||handoffWindow.attempts>=5)){memset(handoffToken,0,sizeof(handoffToken));handoffHash[0]=0;handoffWindow.finish();}xSemaphoreGive(mutex);}}
