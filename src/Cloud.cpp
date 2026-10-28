#include "Cloud.h"
#include "Settings.h"
#include "Events.h"
#include "Engine.h"
#include "Camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_heap_caps.h>
#include <atomic>
namespace Cloud {
static constexpr size_t MAX_IMAGE=128*1024;
struct Config{String url,ca,code,id,token,phone;bool remote=false,wa=false,photo=true,optIn=false;uint16_t interval=60;uint8_t retentionDays=7;int16_t timezoneMinutes=0;};
struct Job{uint32_t event=0,epoch=0;uint8_t score=0;int16_t offset=0;bool test=false;size_t length=0;char key[65]={};};
static Config config;static QueueHandle_t queue;static SemaphoreHandle_t mutex,frameMutex;static uint8_t*image=nullptr;
static std::atomic<bool> enabled{false},pending{false},testPending{false},dirty{false};static uint32_t dropped=0,lastSuccess=0,testCounter=0;static char boot[17]={},phase[24]="not_configured",detail[128]="Pair with your hosted service to enable cloud features",deliveryState[24]="disabled";
static void report(const char*state,const char*message){xSemaphoreTake(mutex,portMAX_DELAY);strlcpy(phase,state,sizeof(phase));strlcpy(detail,message,sizeof(detail));xSemaphoreGive(mutex);}
static Config copy(){xSemaphoreTake(mutex,portMAX_DELAY);Config c=config;xSemaphoreGive(mutex);return c;}
static bool configured(const Config&c){return c.url.length()&&c.ca.length();}
static bool validUrl(const String&s){if(!s.startsWith("https://")||s.length()>180)return false;String host=s.substring(8);if(host.endsWith("/"))host.remove(host.length()-1);if(host.indexOf('.')<1)return false;for(auto ch:host)if(!isalnum((unsigned char)ch)&&ch!='.'&&ch!='-')return false;return true;}
static bool phoneValid(const String&s){if(s.length()<8||s.length()>15||s[0]=='0')return false;for(auto ch:s)if(!isdigit((unsigned char)ch))return false;return true;}
static bool save(const Config&c){DynamicJsonDocument d(6144);if(!d.capacity())return false;d["url"]=c.url;d["ca"]=c.ca;d["id"]=c.id;d["token"]=c.token;d["phone"]=c.phone;d["remote"]=c.remote;d["whatsapp"]=c.wa;d["photo"]=c.photo;d["opt_in"]=c.optIn;d["interval"]=c.interval;d["retention_days"]=c.retentionDays;d["timezone_minutes"]=c.timezoneMinutes;if(d.overflowed())return false;String text;if(serializeJson(d,text)!=measureJson(d))return false;return prefs.getString("cloud","")==text||prefs.putString("cloud",text)>0;}
bool configure(JsonObject o,String&error){
 if(!mutex||!queue){error="Cloud worker unavailable";return false;}Config c=copy();
 if(o.containsKey("url")){String next=o["url"]|"";if(!next.isEmpty()&&!validUrl(next)){error="Use an HTTPS service hostname without a path or port";return false;}if(next.endsWith("/"))next.remove(next.length()-1);if(next!=c.url){c.id="";c.token="";}c.url=next;}
 if(o.containsKey("ca")){String ca=o["ca"]|"";if(ca.length()){if(ca.length()>4096||!ca.startsWith("-----BEGIN CERTIFICATE-----")||ca.indexOf("-----END CERTIFICATE-----")<0){error="Provide the service's PEM root certificate (maximum 4096 bytes)";return false;}c.ca=ca;}}
 for(const char*k:{"remote","whatsapp","photo","opt_in"})if(o.containsKey(k)&&!o[k].is<bool>()){error="Invalid cloud switch";return false;}
 if(o.containsKey("remote"))c.remote=o["remote"];if(o.containsKey("whatsapp"))c.wa=o["whatsapp"];if(o.containsKey("photo"))c.photo=o["photo"];if(o.containsKey("opt_in"))c.optIn=o["opt_in"];
 if(o.containsKey("phone"))c.phone=o["phone"]|"";if(o.containsKey("interval")){if(!o["interval"].is<int>()||o["interval"].as<int>()<30||o["interval"].as<int>()>3600){error="Notification interval must be 30–3600 seconds";return false;}c.interval=o["interval"];}
 if(o.containsKey("retention_days")){if(!o["retention_days"].is<int>()||o["retention_days"].as<int>()<1||o["retention_days"].as<int>()>30){error="Cloud retention must be 1–30 days";return false;}c.retentionDays=o["retention_days"];}
 if(o.containsKey("timezone_minutes")){if(!o["timezone_minutes"].is<int>()||o["timezone_minutes"].as<int>()< -840||o["timezone_minutes"].as<int>()>840){error="Invalid alert timezone";return false;}c.timezoneMinutes=o["timezone_minutes"];}
 if(c.wa&&(!c.optIn||!phoneValid(c.phone))){error="Enter country code plus number (8–15 digits), and confirm recipient opt-in";return false;}
 if(o.containsKey("code")){c.code=o["code"]|"";if(c.code.length()!=0&&c.code.length()!=10){error="Pairing code must contain 10 characters";return false;}if(c.code.length()){c.id="";c.token="";}for(auto ch:c.code)if(!isalnum((unsigned char)ch)){error="Invalid pairing code";return false;}}
 if((c.remote||c.wa)&&(!configured(c)||(!c.token.length()&&!c.code.length()))){error="Service URL, trusted root certificate and a fresh pairing code are required";return false;}
 xSemaphoreTake(mutex,portMAX_DELAY);if(!save(c)){xSemaphoreGive(mutex);error="Cloud settings could not be saved";return false;}config=c;xSemaphoreGive(mutex);enabled=c.remote||c.wa;dirty=true;return true;
}
class LimitedResponse:public Stream{public:String text;size_t write(uint8_t c)override{return write(&c,1);}size_t write(const uint8_t*p,size_t n)override{if(text.length()+n>4096)return 0;return text.concat((const char*)p,n)?n:0;}int available()override{return 0;}int read()override{return -1;}int peek()override{return -1;}void flush()override{}};
static bool request(const Config&c,const char*action,const uint8_t*body,size_t length,JsonDocument&result,const Job*job=nullptr){
 if(WiFi.status()!=WL_CONNECTED||!Engine::epoch()){report("connection_lost","Router Internet and synchronized time are required for HTTPS");return false;}
 if(ESP.getFreeHeap()<65000||heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)<30000){report("waiting","Cloud request skipped to preserve recording memory");return false;}
 WiFiClientSecure tls;tls.setCACert(c.ca.c_str());tls.setHandshakeTimeout(4);tls.setTimeout(5000);HTTPClient http;http.setConnectTimeout(3000);http.setTimeout(5000);http.setReuse(false);http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
 if(!http.begin(tls,c.url+"/api/device?action="+action)){report("connection_lost","HTTPS client could not start");return false;}
 if(c.token.length())http.addHeader("Authorization","Bearer "+c.token);http.addHeader("Content-Type",job&&length?"image/jpeg":"application/json");
 if(job){http.addHeader("X-Event-Id",job->key);http.addHeader("X-Event-Time",String(job->epoch));http.addHeader("X-Event-Score",String(job->score));http.addHeader("X-Timezone-Offset",String(job->offset));http.addHeader("X-Test",job->test?"1":"0");}
 int code=http.POST(const_cast<uint8_t*>(body),length);bool ok=false;
 if(code>=200&&code<300){int n=http.getSize();if(n<=4096){LimitedResponse stream;if(http.writeToStream(&stream)>=0)ok=!deserializeJson(result,stream.text);}}
 http.end();tls.stop();if(!ok){report("connection_lost",code>0?"Cloud rejected the request or returned an invalid response":"HTTPS request failed. Check service, root certificate, clock and Internet.");return false;}xSemaphoreTake(mutex,portMAX_DELAY);lastSuccess=millis()/1000;xSemaphoreGive(mutex);return true;
}
static void task(void*){
 uint32_t nextHeartbeat=0;bool needSync=true;Config c=copy();for(;;){
  bool changed=dirty.exchange(false);if(changed){c=copy();needSync=true;}bool on=c.remote||c.wa;
  if(configured(c)&&(on||needSync)){
   if(c.token.isEmpty()&&c.code.length()){
    report("connecting","Pairing with your service");StaticJsonDocument<256>d;d["code"]=c.code;d["name"]=Engine::cameraName();String body;serializeJson(d,body);StaticJsonDocument<512>r;
    if(request(c,"pair",(const uint8_t*)body.c_str(),body.length(),r)&&r["token"].is<const char*>()&&strlen(r["token"])==64&&r["id"].is<const char*>()){
     xSemaphoreTake(mutex,portMAX_DELAY);if(config.url==c.url){config.token=r["token"].as<String>();config.id=r["id"].as<String>();config.code="";save(config);}xSemaphoreGive(mutex);c=copy();
    }else{report("setup_required","Pairing failed. Generate a fresh code in the remote app.");xSemaphoreTake(mutex,portMAX_DELAY);config.code="";xSemaphoreGive(mutex);c.code="";}
   }
   if(c.token.length()&&(changed||(int32_t)(millis()-nextHeartbeat)>=0)){
    nextHeartbeat=millis()+60000;StaticJsonDocument<512>d;d["name"]=Engine::cameraName();d["remote"]=c.remote;d["whatsapp"]=c.wa;d["photo"]=c.photo;d["phone"]=c.phone;d["opt_in"]=c.optIn;d["interval"]=c.interval;d["retention_days"]=c.retentionDays;d["timezone_minutes"]=c.timezoneMinutes;String body;serializeJson(d,body);StaticJsonDocument<1024>r;
    if(request(c,"heartbeat",(const uint8_t*)body.c_str(),body.length(),r)){needSync=false;report(on?"connected":"disabled",c.wa&&!r["provider_configured"].as<bool>()?"Connected; Meta provider is not configured":"Service connected; delivery is reported separately");String state=r["notification"]["state"]|"disabled";xSemaphoreTake(mutex,portMAX_DELAY);strlcpy(deliveryState,state.c_str(),sizeof(deliveryState));xSemaphoreGive(mutex);Events::delivery(r["notification"]["local_event"]|0,state.c_str());String error=r["notification"]["error"]|"";if(error.length())report("connected",error.c_str());}
   }
  }
  if(on&&(c.remote||c.photo)&&configured(c)&&c.token.length()&&!image&&psramFound()){xSemaphoreTake(frameMutex,portMAX_DELAY);image=(uint8_t*)heap_caps_malloc(MAX_IMAGE,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);xSemaphoreGive(frameMutex);}
  Job j;if(xQueueReceive(queue,&j,pdMS_TO_TICKS(500))==pdTRUE){
   xSemaphoreTake(frameMutex,portMAX_DELAY);if(enabled&&!Engine::paused()&&c.token.length()){
    StaticJsonDocument<512>r;bool sent=false;for(unsigned attempt=0;attempt<3&&enabled&&!sent;attempt++){sent=request(c,"event",j.length?image:(const uint8_t*)"{}",j.length?j.length:2,r,&j);if(!sent)for(unsigned wait=0;wait<(1U<<attempt)*10&&enabled;wait++)delay(100);}
    const char*state=sent?(r["notification"]|"queued"):"upload_failed";Events::delivery(j.event,state);if(sent)report("connected","Event submitted to service; WhatsApp delivery is separate");
   }pending=false;xSemaphoreGive(frameMutex);
  }
  if(!enabled&&image&&!pending){xSemaphoreTake(frameMutex,portMAX_DELAY);free(image);image=nullptr;xSemaphoreGive(frameMutex);}delay(10);
 }
}
void begin(){mutex=xSemaphoreCreateMutex();frameMutex=xSemaphoreCreateMutex();queue=xQueueCreate(1,sizeof(Job));if(!mutex||!frameMutex||!queue)return;strlcpy(boot,randomHex(8).c_str(),sizeof(boot));String saved=prefs.getString("cloud","");if(saved.length()){DynamicJsonDocument d(6144);if(!deserializeJson(d,saved)){config.url=d["url"]|"";config.ca=d["ca"]|"";config.id=d["id"]|"";config.token=d["token"]|"";config.phone=d["phone"]|"";config.remote=d["remote"]|false;config.wa=d["whatsapp"]|false;config.photo=d["photo"]|true;config.optIn=d["opt_in"]|false;config.interval=d["interval"]|60;config.retentionDays=d["retention_days"]|7;config.timezoneMinutes=d["timezone_minutes"]|0;}}enabled=config.remote||config.wa;if(xTaskCreate(task,"cloud",8192,nullptr,1,nullptr)!=pdPASS){vQueueDelete(queue);queue=nullptr;enabled=false;report("unavailable","Cloud worker could not start");}}
bool test(String&err){if(!mutex||!queue){err="Cloud worker unavailable";return false;}Config c=copy();if(!enabled||!c.wa||c.token.isEmpty()){err="Enable and pair WhatsApp first";return false;}if(Engine::paused()||!Camera::ready){err="Resume the camera first";return false;}if(pending||testPending.exchange(true)){err="A notification is already pending";return false;}return true;}
void halt(){enabled=false;testPending=false;dirty=true;if(mutex){xSemaphoreTake(mutex,portMAX_DELAY);config=Config{};xSemaphoreGive(mutex);}}
bool wantsTest(){return testPending.load();}
void offer(camera_fb_t*f,uint32_t event,uint32_t epoch,const char*,uint8_t score,int){
 bool testing=testPending.exchange(false);if(!enabled||(!event&&!testing))return;Config c=copy();if(!c.token.length())return;bool include=c.remote||c.photo;
 if(pending||(include&&f->len>MAX_IMAGE)||xSemaphoreTake(frameMutex,0)!=pdTRUE){xSemaphoreTake(mutex,portMAX_DELAY);dropped++;xSemaphoreGive(mutex);Events::delivery(event,"busy_skipped");return;}
 if(include&&!image){xSemaphoreGive(frameMutex);Events::delivery(event,"busy_skipped");return;}Job j;j.event=event;j.epoch=epoch;j.score=score;j.offset=c.timezoneMinutes;j.test=testing;j.length=include?f->len:0;if(j.length)memcpy(image,f->buf,j.length);if(testing)snprintf(j.key,sizeof(j.key),"test_%s_%lu",boot,(unsigned long)++testCounter);else snprintf(j.key,sizeof(j.key),"event_%lu",(unsigned long)event);
 pending=true;if(xQueueSend(queue,&j,0)!=pdTRUE)pending=false;xSemaphoreGive(frameMutex);Events::delivery(event,pending?"queued":"busy_skipped");
}
void status(JsonObject o){if(!mutex){o["state"]="unavailable";return;}xSemaphoreTake(mutex,portMAX_DELAY);o["state"]=!config.remote&&!config.wa&&config.token.length()?"disabled":phase;o["message"]=detail;o["configured"]=config.token.length()>0;o["remote"]=config.remote;o["whatsapp"]=config.wa;o["photo"]=config.photo;o["opt_in"]=config.optIn;o["phone"]=config.phone;o["interval"]=config.interval;o["retention_days"]=config.retentionDays;o["timezone_minutes"]=config.timezoneMinutes;o["url"]=config.url;o["ca_configured"]=config.ca.length()>0;o["dropped"]=dropped;o["last_success_uptime"]=lastSuccess;o["notification"]=deliveryState;xSemaphoreGive(mutex);}
}
