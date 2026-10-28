#include "Auth.h"
#include "Settings.h"
#include <esp_timer.h>
namespace Auth {
struct Device{char hash[65]={};bool remember=false;int64_t since=0;};
static Device devices[4];static SemaphoreHandle_t mutex;static unsigned next=0;
void begin(){mutex=xSemaphoreCreateMutex();if(!mutex)return;for(int i=0;i<4;i++){String h=prefs.getString((String("device")+i).c_str(),"");if(h.length()==64){strlcpy(devices[i].hash,h.c_str(),65);devices[i].remember=true;}}}
static bool equal(const char*a,const char*b){uint8_t diff=0;for(int i=0;i<64;i++)diff|=a[i]^b[i];return diff==0;}
String issue(bool remember){
 if(!mutex)return "";String token=randomHex(32),hash=passwordHash(token);xSemaphoreTake(mutex,portMAX_DELAY);
 unsigned slot=next++%4;for(unsigned i=0;i<4;i++)if(!devices[i].hash[0]){slot=i;break;}
 String key=String("device")+slot;
 if(remember&&!prefs.putString(key.c_str(),hash)){xSemaphoreGive(mutex);return "";}
 if(!remember)prefs.remove(key.c_str());strlcpy(devices[slot].hash,hash.c_str(),65);devices[slot].remember=remember;devices[slot].since=esp_timer_get_time();xSemaphoreGive(mutex);return token;
}
bool valid(const String&token){
 if(!mutex||token.length()!=64)return false;String hash=passwordHash(token);bool ok=false;xSemaphoreTake(mutex,portMAX_DELAY);
 for(auto&d:devices)if(d.hash[0]&&equal(d.hash,hash.c_str())){ok=d.remember||esp_timer_get_time()-d.since<12LL*60*60*1000000;if(!ok)d.hash[0]=0;break;}
 xSemaphoreGive(mutex);return ok;
}
void revoke(const String&token){if(!mutex||token.length()!=64)return;String hash=passwordHash(token);xSemaphoreTake(mutex,portMAX_DELAY);for(int i=0;i<4;i++)if(devices[i].hash[0]&&equal(devices[i].hash,hash.c_str())){devices[i]=Device{};prefs.remove((String("device")+i).c_str());break;}xSemaphoreGive(mutex);}
}
