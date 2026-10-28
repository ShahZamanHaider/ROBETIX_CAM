#include "Motion.h"
#include "MotionCore.h"
#include <esp_jpg_decode.h>
#include <Arduino.h>
#include <atomic>
#include <esp_heap_caps.h>
#include <new>
namespace Motion {
struct Workspace{MotionCore core;uint32_t sums[3072]={};uint16_t counts[3072]={};uint8_t grid[3072]={};};static Workspace*work=nullptr;
bool available(){return work!=nullptr;}
void begin();
static void allocate(){if(psramFound()){void*p=heap_caps_malloc(sizeof(Workspace),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(p)work=new(p)Workspace;}}
static SemaphoreHandle_t diagnosticsMutex;static uint32_t calibrationUntil=0;static uint8_t diagnosticRegions[48]={};static unsigned diagnosticLocal=0,diagnosticState=0;
void begin(){diagnosticsMutex=xSemaphoreCreateMutex();allocate();}
static std::atomic<uint32_t> decodeUs{0},skipped{0},percent{0},motionScore{0};
uint32_t score(){return motionScore.load();}
struct Context{const uint8_t*data;size_t length;uint16_t width,height;uint32_t start;};
static size_t read(void*arg,size_t index,uint8_t*out,size_t len){auto*c=(Context*)arg;if(index>=c->length)return 0;len=min(len,c->length-index);if(out)memcpy(out,c->data+index,len);return len;}
static bool write(void*arg,uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint8_t*data){
 auto*c=(Context*)arg;if(micros()-c->start>100000)return false;if(!data)return true;
 if(x+w>c->width||y+h>c->height)return false;
 for(unsigned row=0;row<h;row++)for(unsigned col=0;col<w;col++){
  unsigned cell=((y+row)*48/c->height)*64+(x+col)*64/c->width;if(cell>=3072)return false;unsigned pixel=(row*w+col)*3;
  work->sums[cell]+=(77*data[pixel]+150*data[pixel+1]+29*data[pixel+2])>>8;work->counts[cell]++;
 }return true;
}
void reset(){if(work)work->core.reset();}
bool testing(){if(!diagnosticsMutex)return false;xSemaphoreTake(diagnosticsMutex,portMAX_DELAY);bool active=calibrationUntil&&(int32_t)(calibrationUntil-millis())>0;if(!active)calibrationUntil=0;xSemaphoreGive(diagnosticsMutex);return active;}
void calibrate(){if(!diagnosticsMutex)diagnosticsMutex=xSemaphoreCreateMutex();if(diagnosticsMutex){xSemaphoreTake(diagnosticsMutex,portMAX_DELAY);calibrationUntil=millis()+5000;xSemaphoreGive(diagnosticsMutex);}}
void rebase(){if(work)work->core.rebase();}
bool active(uint32_t tailMs){return work&&work->core.active(millis(),tailMs);}
bool analyze(camera_fb_t*f,const AppOptions&o){
 if(!work){skipped++;return false;}
 // Upper bound keeps the ROM decoder and analysis costs predictable.
 if(f->width>800||f->height>600){skipped++;work->core.reset();return false;}
 memset(work->sums,0,sizeof(work->sums));memset(work->counts,0,sizeof(work->counts));Context c={f->buf,f->len,uint16_t(f->width/(f->width<512?4:8)),uint16_t(f->height/(f->width<512?4:8)),micros()};
 esp_err_t err=esp_jpg_decode(f->len,f->width<512?JPG_SCALE_4X:JPG_SCALE_8X,read,write,&c);decodeUs=micros()-c.start;
 if(err!=ESP_OK){skipped++;work->core.reset();return false;}
 for(unsigned i=0;i<3072;i++)work->grid[i]=work->counts[i]?work->sums[i]/work->counts[i]:0;
 bool event=work->core.sample(work->grid,millis(),o.sensitivity,o.minMotionSeconds,o.cooldownSeconds,o.zoneX,o.zoneY,o.zoneW,o.zoneH,o.motionThreshold,o.motionCells,o.motionNeighbors);percent=work->core.changedPercent;motionScore=work->core.localScore;if(diagnosticsMutex){xSemaphoreTake(diagnosticsMutex,portMAX_DELAY);diagnosticLocal=work->core.localScore;diagnosticState=work->core.state;if((int32_t)(calibrationUntil-millis())>0)memcpy(diagnosticRegions,work->core.regions,48);xSemaphoreGive(diagnosticsMutex);}return event;
}
void status(JsonObject o){o["decode_ms"]=decodeUs.load()/1000.0;o["skipped"]=skipped.load();o["changed_percent"]=percent.load();if(diagnosticsMutex){xSemaphoreTake(diagnosticsMutex,portMAX_DELAY);o["state"]=diagnosticState;o["local_score"]=diagnosticLocal;if((int32_t)(calibrationUntil-millis())>0){auto a=o.createNestedArray("regions");for(auto v:diagnosticRegions)a.add(v);}xSemaphoreGive(diagnosticsMutex);}o["grid"]="64x48";o["available"]=available();o["workspace_bytes"]=work?sizeof(Workspace):0;if(!work)o["error"]="Motion analysis needs available PSRAM";o["max_width"]=800;o["max_height"]=600;}
}
