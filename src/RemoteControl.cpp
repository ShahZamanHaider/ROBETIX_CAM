#include "RemoteControl.h"
#include "Engine.h"
#include "Camera.h"
#include "Storage.h"
#include "Events.h"
#include "Light.h"
#include "Live.h"
#include "DeviceStatus.h"
#include <esp_heap_caps.h>
namespace RemoteControl {
// Keep large JSON trees out of the internal heap needed by the TLS connection.
struct JsonAllocator{void*allocate(size_t n){return heap_caps_malloc(n,psramFound()?(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT):MALLOC_CAP_8BIT);}void deallocate(void*p){heap_caps_free(p);}void*reallocate(void*p,size_t n){return heap_caps_realloc(p,n,psramFound()?(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT):MALLOC_CAP_8BIT);}};
using RemoteDocument=BasicJsonDocument<JsonAllocator>;
static SemaphoreHandle_t mutex;static char active[33]={},message[160]={},fileName[32]={};static bool ready=false,success=false;static uint32_t began=0;static uint8_t*chunk=nullptr;
struct Receipt{char id[33]={},message[160]={};bool ok=false;};static Receipt receipts[8];static uint8_t next=0;
static bool validId(const String&id){if(id.length()!=32)return false;for(auto c:id)if(!isxdigit((unsigned char)c))return false;return true;}
static bool reply(String&out,const String&id,bool ok,const char*error,JsonDocument*data=nullptr){RemoteDocument d(data?8192:768);d["type"]="result";d["id"]=id;d["ok"]=ok;if(ok){if(data)d["data"]=data->as<JsonVariantConst>();else d["data"]["message"]=error;}else d["error"]=error;if(d.overflowed()||measureJson(d)>8000){out="{\"type\":\"result\",\"id\":\""+id+"\",\"ok\":false,\"error\":\"Camera response is too large\"}";return true;}serializeJson(d,out);return true;}
void begin(){mutex=xSemaphoreCreateMutex();}
void complete(const char*id,bool ok,const char*text,const char*file){if(!mutex||!id||!*id)return;xSemaphoreTake(mutex,portMAX_DELAY);if(!strcmp(active,id)){success=ok;strlcpy(message,text,sizeof(message));strlcpy(fileName,file?file:"",sizeof(fileName));ready=true;}xSemaphoreGive(mutex);}
bool drain(String&out){if(!mutex)return false;xSemaphoreTake(mutex,portMAX_DELAY);if(*active&&!ready&&millis()-began>12000){ready=true;success=false;strlcpy(message,"Camera action timed out. Check its current state.",sizeof(message));}if(!ready){xSemaphoreGive(mutex);return false;}String id=active,text=message,file=fileName;bool ok=success;auto&r=receipts[next++%8];strlcpy(r.id,active,33);strlcpy(r.message,message,sizeof(r.message));r.ok=ok;active[0]=0;ready=false;xSemaphoreGive(mutex);StaticJsonDocument<384>d;d["message"]=text;if(file.length())d["file"]=file;return reply(out,id,ok,text.c_str(),ok?&d:nullptr);}
bool receive(JsonObject request,String&out){
 String id=request["id"]|"",action=request["action"]|"";JsonObject p=request["params"];if(!validId(id)||p.isNull())return reply(out,id,false,"Invalid command");
 if(action=="status"){RemoteDocument d(7168);DeviceStatus::write(d.to<JsonObject>());if(d.overflowed())return reply(out,id,false,"Camera status is too large");return reply(out,id,true,"",&d);}
 if(action=="events"){RemoteDocument d(6144);Events::list(d.to<JsonObject>());return reply(out,id,true,"",&d);}
 if(action=="files"||action=="file_stat"||action=="file_chunk"||action=="delete"){
  String kind=p["kind"]|"",name=p["name"]|"";if(!Storage::available)return reply(out,id,false,"A working memory card is required");
  if(Engine::occupied()||xSemaphoreTake(Storage::operationMutex,0)!=pdTRUE)return reply(out,id,false,"Stop recording before opening saved files");
  RemoteDocument d(4096);size_t packetBytes=0;bool ok=false;String error="Cannot read saved file";
  if(action=="files")ok=Storage::list(kind,p["before"]|UINT32_MAX,d.to<JsonObject>());
  else if(!Storage::validName(name,kind))error="Invalid file";
  else if(action=="delete"){if(p["confirm"]==true)ok=Storage::remove(name,kind);else error="Confirm file deletion";}
  else{File file=SD_MMC.open(Storage::path(name,kind),FILE_READ);if(file){uint32_t size=file.size();if(action=="file_stat"){d["name"]=name;d["kind"]=kind;d["size"]=size;ok=size>0;}else{
     uint32_t offset=p["offset"]|UINT32_MAX;size_t capacity=psramFound()?8192:1024;size_t count=min(uint32_t(capacity),p["limit"]|uint32_t(capacity));if(!chunk)chunk=(uint8_t*)heap_caps_malloc(capacity+32,psramFound()?(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT):MALLOC_CAP_8BIT);
     if(!chunk)error="File buffer unavailable";else if(offset>=size||!count||!file.seek(offset))error="Invalid file offset";else{count=min(size-offset,uint32_t(count));size_t got=file.read(chunk+32,count);if(got==count){memcpy(chunk,"RVF1",4);for(int i=0;i<16;i++)chunk[4+i]=strtoul(id.substring(i*2,i*2+2).c_str(),nullptr,16);for(int i=0;i<4;i++){chunk[20+i]=offset>>(24-8*i);chunk[24+i]=size>>(24-8*i);}chunk[28]=count>>8;chunk[29]=count;chunk[30]=chunk[31]=0;packetBytes=count+32;ok=true;}else error="Memory card read failed";}
    }file.close();}}
  xSemaphoreGive(Storage::operationMutex);
  if(action=="file_chunk"&&ok){if(Live::sendPacket(chunk,packetBytes))return false;return reply(out,id,false,"File connection interrupted");}
  return reply(out,id,ok,error.c_str(),ok?&d:nullptr);
 }
 if(!mutex)return reply(out,id,false,"Camera controls are unavailable");
 xSemaphoreTake(mutex,portMAX_DELAY);for(auto&r:receipts)if(!strcmp(r.id,id.c_str())){bool ok=r.ok;String text=r.message;xSemaphoreGive(mutex);return reply(out,id,ok,text.c_str());}if(*active){bool duplicate=id==active;xSemaphoreGive(mutex);return duplicate?false:reply(out,id,false,"Camera is busy. Try again shortly.");}strlcpy(active,id.c_str(),33);began=millis();ready=false;xSemaphoreGive(mutex);
 Engine::Command c;strlcpy(c.remoteId,id.c_str(),sizeof(c.remoteId));bool accepted=false;String error;
 if(action=="record"){c.action=Engine::Action::Start;accepted=Engine::submit(c);}
 else if(action=="stop"){c.action=Engine::Action::Stop;accepted=Engine::submit(c);}
 else if(action=="photo"){c.action=Engine::Action::Photo;accepted=Engine::submit(c);}
 else if(action=="light"&&p["brightness"].is<int>()&&p["brightness"].as<int>()>=0&&p["brightness"].as<int>()<=100){c.action=Engine::Action::Light;c.camera[0]=p["brightness"];accepted=Engine::submit(c);}
 else if(action=="privacy"&&p["enabled"].is<bool>()){c.action=Engine::Action::Privacy;c.flag=p["enabled"];accepted=Engine::submit(c);}
 else if(action=="restart"){c.action=Engine::Action::Restart;accepted=Engine::submit(c);}
 else if(action=="options")accepted=Engine::configure(p,error,id.c_str());
 else if(action=="light_preferences"){bool ok=p["preferred"].is<int>()&&p["auto_off"].is<int>()&&p["preferred"].as<int>()>=0&&p["preferred"].as<int>()<=100&&p["auto_off"].as<int>()>=10&&p["auto_off"].as<int>()<=300&&Light::configure(p["preferred"],p["auto_off"]);complete(id.c_str(),ok,ok?"Light preferences saved":"Invalid light settings");return drain(out);}
 else if(action=="camera"){
  if(Camera::ready&&xSemaphoreTake(Camera::mutex,pdMS_TO_TICKS(50))==pdTRUE){auto s=esp_camera_sensor_get()->status;c.action=Engine::Action::Camera;c.camera[0]=p["size"]|int(s.framesize);c.camera[1]=p["quality"]|s.quality;c.camera[2]=p["brightness"]|s.brightness;c.camera[3]=p["contrast"]|s.contrast;c.camera[4]=p["saturation"]|s.saturation;c.camera[5]=p["mirror"]|s.hmirror;c.camera[6]=p["flip"]|s.vflip;c.camera[7]=p["white_balance"]|s.wb_mode;c.camera[8]=p["exposure"]|(s.aec?-1:s.aec_value);c.camera[9]=p["gain"]|(s.agc?-1:s.agc_gain);xSemaphoreGive(Camera::mutex);accepted=Engine::submit(c);}
 }
 if(!accepted){complete(id.c_str(),false,error.length()?error.c_str():"Camera rejected the command");return drain(out);}return false;
}
}
