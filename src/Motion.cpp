#include "Motion.h"
#include "MotionCore.h"
#include <esp_jpg_decode.h>
#include <Arduino.h>
#include <atomic>
namespace Motion {
static MotionCore core;static uint32_t sums[768];static uint16_t counts[768];static uint8_t grid[768];
static std::atomic<uint32_t> decodeUs{0},skipped{0},percent{0};
struct Context{const uint8_t*data;size_t length;uint16_t width,height;uint32_t start;};
static size_t read(void*arg,size_t index,uint8_t*out,size_t len){auto*c=(Context*)arg;if(index>=c->length)return 0;len=min(len,c->length-index);if(out)memcpy(out,c->data+index,len);return len;}
static bool write(void*arg,uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint8_t*data){
 auto*c=(Context*)arg;if(micros()-c->start>100000)return false;if(!data)return true;
 if(x+w>c->width||y+h>c->height)return false;
 for(unsigned row=0;row<h;row++)for(unsigned col=0;col<w;col++){
  unsigned cell=((y+row)*24/c->height)*32+(x+col)*32/c->width;if(cell>=768)return false;unsigned pixel=(row*w+col)*3;
  sums[cell]+=(77*data[pixel]+150*data[pixel+1]+29*data[pixel+2])>>8;counts[cell]++;
 }return true;
}
void reset(){core.reset();}
void rebase(){core.rebase();}
bool active(uint32_t tailMs){return core.active(millis(),tailMs);}
bool analyze(camera_fb_t*f,const AppOptions&o){
 // Upper bound keeps the ROM decoder and analysis costs predictable.
 if(f->width>800||f->height>600){skipped++;core.reset();return false;}
 memset(sums,0,sizeof(sums));memset(counts,0,sizeof(counts));Context c={f->buf,f->len,uint16_t(f->width/8),uint16_t(f->height/8),micros()};
 esp_err_t err=esp_jpg_decode(f->len,JPG_SCALE_8X,read,write,&c);decodeUs=micros()-c.start;
 if(err!=ESP_OK){skipped++;core.reset();return false;}
 for(unsigned i=0;i<768;i++)grid[i]=counts[i]?sums[i]/counts[i]:0;
 bool event=core.sample(grid,millis(),o.sensitivity,o.minMotionSeconds,o.cooldownSeconds,o.zoneX,o.zoneY,o.zoneW,o.zoneH);percent=core.changedPercent;return event;
}
void status(JsonObject o){o["decode_ms"]=decodeUs.load()/1000.0;o["skipped"]=skipped.load();o["changed_percent"]=percent.load();o["max_width"]=800;o["max_height"]=600;}
}
