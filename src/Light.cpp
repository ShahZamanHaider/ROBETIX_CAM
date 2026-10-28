#include "Light.h"
#include <atomic>
#include "Settings.h"
// GPIO4 is unused by SD_MMC 1-bit. Arduino channel 6 maps to LEDC timer 3;
// camera XCLK uses timer 0/channel 0. Caps are conservative, not thermal measurements.
namespace Light {
static constexpr int CHANNEL=6;static std::atomic<uint8_t> preferred{40};static std::atomic<uint16_t> timer{60};static std::atomic<uint8_t> level{0};static std::atomic<uint32_t> until{0};
void begin(){preferred=min(uint8_t(100),prefs.getUChar("lightPref",40));timer=constrain(prefs.getUShort("lightTimer",60),10,300);ledcSetup(CHANNEL,5000,8);ledcAttachPin(4,CHANNEL);ledcWrite(CHANNEL,0);}
void set(uint8_t percent){level=min(percent,uint8_t(100));ledcWrite(CHANNEL,(uint32_t(level)*255*25)/10000);until=millis()+uint32_t(timer)*1000;}
void tick(){if(level&&(int32_t)(millis()-until)>=0)set(0);}
uint8_t value(){return level;}
bool configure(uint8_t p,uint16_t seconds){if(p>100||seconds<10||seconds>300)return false;bool ok=true;if(preferred!=p)ok=prefs.putUChar("lightPref",p)>0;if(timer!=seconds)ok=(prefs.putUShort("lightTimer",seconds)>0)&&ok;if(ok){preferred=p;timer=seconds;}return ok;}
void status(JsonObject o){o["level"]=value();o["preferred"]=preferred.load();o["auto_off"]=timer.load();o["hardware_cap"]=25;o["duty_percent"]=((uint32_t(value())*255*25)/10000)*100.0/255;}
}
