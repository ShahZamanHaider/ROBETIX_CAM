#include "Maintenance.h"
#include "Storage.h"
#include "Settings.h"
#include "Engine.h"
#include <atomic>
namespace Maintenance {
static std::atomic<bool> resetting{false};
void pauseForReset(){resetting=true;}
struct Job{bool erase=false,automatic=false;char kind[8]="videos";uint32_t from=0,to=0,maxId=UINT32_MAX;};
struct Report{char phase[16]="idle",error[128]={},token[33]={};uint32_t scanned=0,count=0,deleted=0,skipped=0,first=0,last=0,maxId=0;uint64_t bytes=0;} report;
static SemaphoreHandle_t mutex;static QueueHandle_t queue;static Job selection;static AppOptions rules;static char logs[4][96]={};static unsigned logIndex=0;
static bool busy(){return !strcmp(report.phase,"queued")||!strcmp(report.phase,"finding")||!strcmp(report.phase,"deleting");}
void policy(const AppOptions&o){if(!mutex)return;xSemaphoreTake(mutex,portMAX_DELAY);rules=o;xSemaphoreGive(mutex);}
bool find(const String&kind,uint32_t from,uint32_t to,String&error){
 if(!mutex||!queue){error="Storage manager unavailable";return false;}
 if((kind!="videos"&&kind!="photos")||from<1704067200UL||to>4102444800UL||from>to){error="Choose a valid date range";return false;}
 if(Engine::occupied()){error="Stop recording before searching dates";return false;}
 Job j;strlcpy(j.kind,kind.c_str(),sizeof(j.kind));j.from=from;j.to=to;
 xSemaphoreTake(mutex,portMAX_DELAY);if(busy()){xSemaphoreGive(mutex);error="Storage operation already running";return false;}report=Report{};strlcpy(report.phase,"queued",16);selection=j;bool ok=xQueueSend(queue,&j,0)==pdTRUE;if(!ok)strlcpy(report.phase,"idle",16);xSemaphoreGive(mutex);return ok;
}
bool confirm(const String&token,String&error){
 if(!mutex||!queue)return false;xSemaphoreTake(mutex,portMAX_DELAY);
 if(strcmp(report.phase,"ready")||!report.count||token!=report.token||token.length()!=32){xSemaphoreGive(mutex);error="Search again before deleting";return false;}
 Job j=selection;j.erase=true;j.maxId=report.maxId;strlcpy(report.phase,"queued",16);report.token[0]=0;bool ok=xQueueSend(queue,&j,0)==pdTRUE;if(!ok)strlcpy(report.phase,"error",16);xSemaphoreGive(mutex);return ok;
}
static void publish(const Report&r){xSemaphoreTake(mutex,portMAX_DELAY);report=r;xSemaphoreGive(mutex);}
static void run(Job j){
 Report r;strlcpy(r.phase,j.erase?"deleting":"finding",16);publish(r);
 if(!Storage::available||xSemaphoreTake(Storage::operationMutex,0)!=pdTRUE){strlcpy(r.phase,"error",16);strlcpy(r.error,"Memory card busy or unavailable. Stop recording and try again.",128);publish(r);return;}
 File dir=SD_MMC.open(String("/robetix/")+j.kind);if(!dir){strlcpy(r.error,"Could not read folder",128);}
 else while(!resetting){File f=dir.openNextFile();if(!f)break;
  String name=f.name();int slash=name.lastIndexOf('/');if(slash>=0)name=name.substring(slash+1);bool valid=!f.isDirectory()&&Storage::validName(name,j.kind);uint32_t bytes=f.size();f.close();
  if(++r.scanned>4096){strlcpy(r.error,"Directory scan limit reached. Use a narrower library or a card reader.",128);break;}
  uint32_t created=valid?strtoul(name.c_str()+14,nullptr,10):0,id=valid?Storage::sequence(name):0;
  bool match=valid&&created>=1704067200UL&&created<=4102444800UL&&created>=j.from&&created<=j.to&&id<=j.maxId;
  if(match){r.count++;r.bytes+=bytes;r.maxId=max(r.maxId,id);if(!r.first||created<r.first)r.first=created;r.last=max(r.last,created);
   if(j.erase&&!resetting){if(!SD_MMC.remove(Storage::path(name,j.kind))){strlcpy(r.error,"A deletion failed. Remaining files were left in place.",128);break;}r.deleted++;}
  }else r.skipped++;
  if(!(r.scanned%8)){publish(r);delay(10);}
 }
 dir.close();if(j.erase)Storage::refresh();xSemaphoreGive(Storage::operationMutex);
 strlcpy(r.phase,*r.error?"error":j.erase?"done":"ready",16);
 if(!j.erase&&!*r.error)strlcpy(r.token,randomHex(16).c_str(),33);
 publish(r);if(j.erase){xSemaphoreTake(mutex,portMAX_DELAY);snprintf(logs[logIndex++%4],96,"%s: deleted %lu, skipped %lu%s",j.kind,(unsigned long)r.deleted,(unsigned long)r.skipped,*r.error?"; stopped on error":"");xSemaphoreGive(mutex);}
}
static void task(void*){
 uint32_t nextRetention=millis()+60000;
 for(;;){Job j;if(xQueueReceive(queue,&j,pdMS_TO_TICKS(1000))==pdTRUE){run(j);continue;}
  if(resetting||(int32_t)(millis()-nextRetention)<0||Engine::occupied())continue;nextRetention=millis()+600000;uint32_t now=Engine::epoch();if(!now)continue;
  xSemaphoreTake(mutex,portMAX_DELAY);AppOptions v=rules;bool waiting=!strcmp(report.phase,"ready")||busy();xSemaphoreGive(mutex);if(waiting)continue;
  for(int type=0;type<2;type++){bool enabled=type?v.retainPhotos:v.retainVideos;uint32_t days=type?v.photoDays:v.videoDays;if(!enabled||now<=1704067200UL+days*86400UL)continue;Job cleanup;cleanup.erase=true;cleanup.automatic=true;strlcpy(cleanup.kind,type?"photos":"videos",8);cleanup.from=1704067200UL;cleanup.to=now-days*86400UL-1;run(cleanup);}
 }
}
void begin(){mutex=xSemaphoreCreateMutex();queue=xQueueCreate(1,sizeof(Job));if(mutex&&queue&&xTaskCreate(task,"storage-jobs",4096,nullptr,1,nullptr)!=pdPASS){vQueueDelete(queue);queue=nullptr;}}
void status(JsonObject o){if(!mutex){o["phase"]="unavailable";return;}xSemaphoreTake(mutex,portMAX_DELAY);o["phase"]=report.phase;o["error"]=report.error;o["token"]=report.token;o["scanned"]=report.scanned;o["count"]=report.count;o["bytes"]=report.bytes;o["deleted"]=report.deleted;o["skipped"]=report.skipped;o["first"]=report.first;o["last"]=report.last;o["kind"]=selection.kind;auto a=o.createNestedArray("log");for(unsigned i=0;i<min(logIndex,4U);i++)a.add(logs[(logIndex-1-i)%4]);xSemaphoreGive(mutex);}
}
