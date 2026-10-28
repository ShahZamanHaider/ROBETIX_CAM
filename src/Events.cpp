#include "Events.h"
#include "Storage.h"
namespace Events {
struct Event{uint32_t magic=0x52564532,id=0,epoch=0,uptime=0;char photo[29]={},video[29]={},note[64]={};uint8_t ready=0;uint8_t padding[1]={};uint32_t checksum=0;};
static constexpr unsigned LIMIT=32;static Event events[LIMIT];static uint32_t next=0;static SemaphoreHandle_t mutex;
static uint32_t checksum(const Event&e){uint32_t h=2166136261;const uint8_t*p=(const uint8_t*)&e;for(size_t i=0;i<offsetof(Event,checksum);i++)h=(h^p[i])*16777619;return h;}
static void persist(Event e,bool owned){
 if(!Storage::available)return;bool lock=owned||xSemaphoreTake(Storage::operationMutex,0)==pdTRUE;if(!lock)return;e.checksum=checksum(e);
 File f=SD_MMC.open("/robetix/events.bin",SD_MMC.exists("/robetix/events.bin")?"r+":"w+");if(f){if(f.seek((e.id%LIMIT)*sizeof(Event)))f.write((uint8_t*)&e,sizeof(e));f.flush();f.close();}if(!owned)xSemaphoreGive(Storage::operationMutex);
}
void begin(){mutex=xSemaphoreCreateMutex();if(!mutex||!Storage::available)return;File f=SD_MMC.open("/robetix/events.bin");if(!f)return;for(unsigned i=0;i<LIMIT;i++){Event e;if(f.read((uint8_t*)&e,sizeof(e))!=sizeof(e))break;if(e.magic==0x52564532&&e.id&&e.checksum==checksum(e)&&e.photo[28]==0&&e.video[28]==0&&e.note[63]==0){events[e.id%LIMIT]=e;next=max(next,e.id);}}f.close();}
uint32_t add(uint32_t epoch,const String&photo,const String&video,const char*note,bool owned){if(!mutex)return 0;Event e;e.epoch=epoch;e.uptime=millis()/1000;strlcpy(e.photo,photo.c_str(),sizeof(e.photo));strlcpy(e.video,video.c_str(),sizeof(e.video));strlcpy(e.note,note,sizeof(e.note));xSemaphoreTake(mutex,portMAX_DELAY);e.id=++next;events[e.id%LIMIT]=e;xSemaphoreGive(mutex);persist(e,owned);return e.id;}
void attach(uint32_t id,const String&video,bool owned){if(!mutex||!id)return;xSemaphoreTake(mutex,portMAX_DELAY);Event e=events[id%LIMIT];if(e.id==id){strlcpy(e.video,video.c_str(),sizeof(e.video));events[id%LIMIT]=e;}xSemaphoreGive(mutex);if(e.id==id)persist(e,owned);}
void complete(const String&video,bool ok,bool owned){if(!mutex)return;for(unsigned i=0;i<LIMIT;i++){xSemaphoreTake(mutex,portMAX_DELAY);Event e=events[i];bool match=e.id&&video==e.video;if(match){e.ready=ok?1:0;strlcpy(e.note,ok?"Movement detected":"Movement detected; video save failed",sizeof(e.note));events[i]=e;}xSemaphoreGive(mutex);if(match)persist(e,owned);}}
uint32_t latest(){if(!mutex)return 0;xSemaphoreTake(mutex,portMAX_DELAY);uint32_t id=next;xSemaphoreGive(mutex);return id;}
void list(JsonObject o){auto a=o.createNestedArray("events");if(!mutex)return;xSemaphoreTake(mutex,portMAX_DELAY);for(uint32_t id=next;id&&id+12>next;id--){Event&e=events[id%LIMIT];if(e.id!=id)continue;auto row=a.createNestedObject();row["id"]=e.id;row["epoch"]=e.epoch;row["uptime"]=e.uptime;row["photo"]=e.photo;row["video"]=e.video;row["video_ready"]=bool(e.ready);row["note"]=e.note;}xSemaphoreGive(mutex);}
}
