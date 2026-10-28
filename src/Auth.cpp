#include "Auth.h"
#include "Settings.h"
#include "RecoveryWindow.h"
#include <esp_timer.h>
namespace Auth {
struct Device{char hash[65]={};bool remember=false;int64_t since=0;};
static Device devices[4];static SemaphoreHandle_t mutex;static unsigned next=0;
static char recoveryHash[65]={};static RecoveryWindow recoveryWindow;
void begin(){mutex=xSemaphoreCreateMutex();if(!mutex)return;for(int i=0;i<4;i++){String h=prefs.getString((String("device")+i).c_str(),"");if(h.length()==64){strlcpy(devices[i].hash,h.c_str(),65);devices[i].remember=true;}}}
static bool equal(const char*a,const char*b){uint8_t diff=0;for(int i=0;i<64;i++)diff|=a[i]^b[i];return diff==0;}
String issue(bool remember){
 if(!mutex)return "";String token=randomHex(32);xSemaphoreTake(mutex,portMAX_DELAY);String hash=passwordHash(token);
 unsigned slot=next++%4;for(unsigned i=0;i<4;i++)if(!devices[i].hash[0]){slot=i;break;}
 String key=String("device")+slot;
 if(remember&&!prefs.putString(key.c_str(),hash)){xSemaphoreGive(mutex);return "";}
 if(!remember)prefs.remove(key.c_str());strlcpy(devices[slot].hash,hash.c_str(),65);devices[slot].remember=remember;devices[slot].since=esp_timer_get_time();xSemaphoreGive(mutex);return token;
}
bool valid(const String&token){
 if(!mutex||token.length()!=64)return false;bool ok=false;xSemaphoreTake(mutex,portMAX_DELAY);String hash=passwordHash(token);
 for(auto&d:devices)if(d.hash[0]&&equal(d.hash,hash.c_str())){ok=d.remember||esp_timer_get_time()-d.since<12LL*60*60*1000000;if(!ok)d.hash[0]=0;break;}
 xSemaphoreGive(mutex);return ok;
}
void revoke(const String&token){if(!mutex||token.length()!=64)return;xSemaphoreTake(mutex,portMAX_DELAY);String hash=passwordHash(token);for(int i=0;i<4;i++)if(devices[i].hash[0]&&equal(devices[i].hash,hash.c_str())){devices[i]=Device{};prefs.remove((String("device")+i).c_str());break;}xSemaphoreGive(mutex);}
String armRecovery(){if(!mutex)return "";String code=randomHex(16);xSemaphoreTake(mutex,portMAX_DELAY);strlcpy(recoveryHash,passwordHash(code).c_str(),sizeof(recoveryHash));recoveryWindow.start(esp_timer_get_time());xSemaphoreGive(mutex);return code;}
bool recover(const String&code,const String&password,String&error){if(!mutex)return false;xSemaphoreTake(mutex,portMAX_DELAY);bool ok=false;
 if(code.length()!=32||!recoveryHash[0]||!recoveryWindow.permit(esp_timer_get_time())){error="Recovery code is invalid or expired. Request a new code using USB.";}
 else{if(!equal(recoveryHash,passwordHash(code).c_str()))error="Recovery code is invalid or expired. Request a new code using USB.";else if(password.length()<12||password.length()>64)error="Choose a 12–64 character administrator password";else if(!setAdminPassword(password))error="Settings write failed";else{recoveryHash[0]=0;recoveryWindow.finish();for(int i=0;i<4;i++){devices[i]=Device{};prefs.remove((String("device")+i).c_str());}ok=true;}}
 xSemaphoreGive(mutex);return ok;
}
}
