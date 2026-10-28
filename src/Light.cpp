#include "Light.h"
#include <atomic>
// GPIO4 is unused by SD_MMC 1-bit. Arduino channel 6 maps to LEDC timer 3;
// camera XCLK uses timer 0/channel 0. Caps are conservative, not thermal measurements.
namespace Light {
static constexpr int CHANNEL=6;static std::atomic<uint8_t> level{0};static std::atomic<uint32_t> until{0};
void begin(){ledcSetup(CHANNEL,5000,8);ledcAttachPin(4,CHANNEL);ledcWrite(CHANNEL,0);}
void set(uint8_t percent){level=min(percent,uint8_t(25));ledcWrite(CHANNEL,(uint32_t(level)*255)/100);until=millis()+60000;}
void tick(){if(level&&(int32_t)(millis()-until)>=0)set(0);}
uint8_t value(){return level;}
}
