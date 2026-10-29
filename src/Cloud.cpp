#include "Cloud.h"
#include "Live.h"
#include "TlsRoots.h"
#include "VerifiedTLS.h"
#include "Settings.h"
#include "ServiceOrigin.h"
#include "Network.h"
#include "PairingPolicy.h"
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
static std::atomic<bool> resetRequested{false},resetDone{false},resetVerified{false};
struct Config{String url,ca,code,id,token,phone,requestId;uint32_t revision=0,pairEpoch=0;bool remote=false,wa=false,photo=true,optIn=false;uint16_t interval=60;uint8_t retentionDays=7;int16_t timezoneMinutes=0;};
struct Job{uint32_t event=0,epoch=0;uint8_t score=0;int16_t offset=0;bool test=false;size_t length=0;char key[65]={};};
static std::atomic<uint32_t> nameRevision{0};static uint32_t nameAck=0;
static Config config;static PairingPolicy pairing;static std::atomic<bool> credentialsPending{false};static std::atomic<int> lastHTTP{0};static char pairState[24]="idle";static QueueHandle_t queue;static SemaphoreHandle_t mutex,frameMutex;static uint8_t*image=nullptr;
static std::atomic<bool> enabled{false},pending{false},testPending{false},dirty{false};static bool providerReady=false,notificationsPaused=false,nativeServiceReady=false,pairVerified=false;static uint32_t dropped=0,lastSuccess=0,testCounter=0;static char boot[17]={},phase[24]="not_configured",detail[128]="Pair with your hosted service to enable cloud features",deliveryState[24]="disabled";
static void report(const char*state,const char*message){xSemaphoreTake(mutex,portMAX_DELAY);strlcpy(phase,state,sizeof(phase));strlcpy(detail,message,sizeof(detail));xSemaphoreGive(mutex);}
static Config copy(){xSemaphoreTake(mutex,portMAX_DELAY);Config c=config;xSemaphoreGive(mutex);return c;}
static bool configured(const Config&c){return c.url.length();}
static bool validUrl(const String&s){if(!s.startsWith("https://")||s.length()>180)return false;String host=s.substring(8);if(host.endsWith("/"))host.remove(host.length()-1);if(host.indexOf('.')<1)return false;for(auto ch:host)if(!isalnum((unsigned char)ch)&&ch!='.'&&ch!='-')return false;return true;}
static bool save(const Config&c){DynamicJsonDocument d(6144);if(!d.capacity())return false;d["pair_code"]=c.code;d["pair_request"]=c.requestId;d["pair_epoch"]=c.pairEpoch;d["url"]=c.url;d["ca"]=c.ca;d["id"]=c.id;d["token"]=c.token;d["phone"]=c.phone;d["managed_recipients"]=true;d["remote"]=c.remote;d["whatsapp"]=c.wa;d["photo"]=c.photo;d["opt_in"]=c.optIn;d["interval"]=c.interval;d["retention_days"]=c.retentionDays;d["timezone_minutes"]=c.timezoneMinutes;if(d.overflowed())return false;String text;if(serializeJson(d,text)!=measureJson(d))return false;return prefs.getString("cloud","")==text||prefs.putString("cloud",text)>0;}
bool configure(JsonObject o,String&error){
 if(!mutex||!queue){error="Cloud worker unavailable";return false;}Config c=copy();
 if(o["simple_pair"]==true){if(c.url!=ServiceOrigin::url){c.id="";c.token="";}c.url=ServiceOrigin::url;c.ca="";c.remote=true;}
 if(o.containsKey("url")){String next=o["url"]|"";if(!next.isEmpty()&&!validUrl(next)){error="Use an HTTPS service hostname without a path or port";return false;}if(next.endsWith("/"))next.remove(next.length()-1);if(next!=c.url){c.id="";c.token="";}c.url=next;}
 if(o.containsKey("ca")){String ca=o["ca"]|"";if(ca.length()){if(ca.length()>4096||!ca.startsWith("-----BEGIN CERTIFICATE-----")||ca.indexOf("-----END CERTIFICATE-----")<0){error="Provide the service's PEM root certificate (maximum 4096 bytes)";return false;}c.ca=ca;}}
 for(const char*k:{"remote","whatsapp","photo","opt_in"})if(o.containsKey(k)&&!o[k].is<bool>()){error="Invalid cloud switch";return false;}
 if(o.containsKey("remote"))c.remote=o["remote"];if(o.containsKey("whatsapp"))c.wa=o["whatsapp"];if(o.containsKey("photo"))c.photo=o["photo"];if(o.containsKey("opt_in"))c.optIn=o["opt_in"];
 if(o.containsKey("phone"))c.phone=o["phone"]|"";if(o.containsKey("interval")){if(!o["interval"].is<int>()||o["interval"].as<int>()<30||o["interval"].as<int>()>3600){error="Notification interval must be 30–3600 seconds";return false;}c.interval=o["interval"];}
 if(o.containsKey("retention_days")){if(!o["retention_days"].is<int>()||o["retention_days"].as<int>()<1||o["retention_days"].as<int>()>30){error="Cloud retention must be 1–30 days";return false;}c.retentionDays=o["retention_days"];}
 if(o.containsKey("timezone_minutes")){if(!o["timezone_minutes"].is<int>()||o["timezone_minutes"].as<int>()< -840||o["timezone_minutes"].as<int>()>840){error="Invalid alert timezone";return false;}c.timezoneMinutes=o["timezone_minutes"];}
 // Recipient identity and consent are managed on the authenticated HTTPS backend.
 if(o.containsKey("code")){String next=o["code"]|"";next.toUpperCase();if(next.length()!=0&&next.length()!=10){error="Pairing code must contain 10 characters";return false;}for(auto ch:next)if(!isxdigit((unsigned char)ch)){error="Invalid pairing code";return false;}if(next!=c.code){c.requestId=next.length()?randomHex(16):String("");c.pairEpoch=Engine::epoch();}c.code=next;if(c.code.length()){c.id="";c.token="";}}

 if((c.remote||c.wa)&&(!configured(c)||(!c.token.length()&&!c.code.length()))){error="A fresh pairing code is required";return false;}
 xSemaphoreTake(mutex,portMAX_DELAY);c.revision=config.revision+1;if(!save(c)){xSemaphoreGive(mutex);error="Cloud settings could not be saved";return false;}config=c;credentialsPending=false;if(c.token.isEmpty()){pairVerified=false;nativeServiceReady=false;}strlcpy(pairState,c.code.length()?"waiting":"idle",sizeof(pairState));xSemaphoreGive(mutex);enabled=c.remote||c.wa;dirty=true;return true;
}
class LimitedResponse:public Stream{public:String text;size_t write(uint8_t c)override{return write(&c,1);}size_t write(const uint8_t*p,size_t n)override{if(text.length()+n>4096)return 0;return text.concat((const char*)p,n)?n:0;}int available()override{return 0;}int read()override{return -1;}int peek()override{return -1;}void flush()override{}};
static bool request(const Config&c,const char*action,const uint8_t*body,size_t length,JsonDocument&result,const Job*job=nullptr,int*status=nullptr){
 if(status)*status=0;if(!Network::connected()||!Engine::epoch()){report("waiting","Connect to home Wi-Fi and open this camera page to set its clock.");return false;}
 Live::close();
 if(ESP.getFreeHeap()<65000||heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)<30000){report("waiting","Cloud request skipped to preserve recording memory");return false;}
 VerifiedTLS tls;if(c.ca.length())tls.setCACert(c.ca.c_str());else tls.setCACert(ROBETIX_SERVICE_ROOTS);tls.setHandshakeTimeout(4);tls.setTimeout(5);HTTPClient http;http.setConnectTimeout(3000);http.setTimeout(5000);http.setReuse(false);http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
 if(!http.begin(tls,c.url+"/api/device?action="+action)){report("connection_lost","HTTPS client could not start");return false;}
 if(c.token.length())http.addHeader("Authorization","Bearer "+c.token);http.addHeader("Content-Type",job&&length?"image/jpeg":"application/json");
 if(job){http.addHeader("X-Event-Id",job->key);http.addHeader("X-Event-Time",String(job->epoch));http.addHeader("X-Event-Score",String(job->score));http.addHeader("X-Timezone-Offset",String(job->offset));http.addHeader("X-Test",job->test?"1":"0");}
 int code=http.POST(const_cast<uint8_t*>(body),length);lastHTTP=code;if(code<0)Serial.printf("ROBETIX_TLS_DIAGNOSTIC: code=%d flags=%lu epoch=%lu\n",tls.result,(unsigned long)tls.verification,(unsigned long)Engine::epoch());if(status)*status=code;bool ok=false;
 LimitedResponse stream;if(code>0&&http.getSize()<=4096&&http.writeToStream(&stream)>=0){bool parsed=!deserializeJson(result,stream.text);ok=code>=200&&code<300&&parsed;}
 http.end();tls.stop();if(!ok){if(code==401&&!strcmp(action,"heartbeat")){xSemaphoreTake(mutex,portMAX_DELAY);pairVerified=false;nativeServiceReady=false;xSemaphoreGive(mutex);}String message=result["error"]|"";if(message.length()>120)message="Website request failed. Check the connection and try again.";report("connection_lost",message.length()?message.c_str():code==401||code==403?"Website rejected this camera. Check pairing and test website access.":code>0?"Website returned an unexpected response. Check the selected camera website.":tls.verification?"Secure connection to the website failed. Local viewing still works.":"Internet connection failed. The camera will retry.");return false;}xSemaphoreTake(mutex,portMAX_DELAY);lastSuccess=millis()/1000;xSemaphoreGive(mutex);return true;

}
static void task(void*){
 uint32_t nextHeartbeat=0,saveRetry=0,nextLiveTicket=0;bool needSync=true;Config c=copy();for(;;){
  if(resetRequested){if(!resetDone){Config current=copy();StaticJsonDocument<256>response;int status=0;bool ok=current.token.isEmpty()||(Network::connected()&&request(current,"factory-reset",(const uint8_t*)"{}",2,response,nullptr,&status)&&response["credentials_revoked"]==true);resetVerified=ok||status==401;Live::close();resetDone=true;}delay(20);continue;}
  bool changed=dirty.exchange(false);if(changed){c=copy();needSync=true;nextHeartbeat=0;pairing.start(millis());}bool on=c.remote||c.wa;
  if(configured(c)&&(on||needSync)){
   if(c.token.isEmpty()&&c.code.length()){
    if(!pairing.active)pairing.start(millis());
    bool expired=pairing.expired(millis())||(c.pairEpoch&&Engine::epoch()>c.pairEpoch&&Engine::epoch()-c.pairEpoch>=300);
    if(expired){xSemaphoreTake(mutex,portMAX_DELAY);if(config.revision==c.revision){config.code="";config.requestId="";save(config);strlcpy(pairState,"expired",sizeof(pairState));}xSemaphoreGive(mutex);c=copy();pairing.done();report("setup_required","Pairing code expired. Get a new code from your account.");}
    else if(Network::connected()&&Engine::epoch()&&pairing.ready(millis())){
     report("connecting","Connecting this camera to your account…");xSemaphoreTake(mutex,portMAX_DELAY);strlcpy(pairState,"connecting",sizeof(pairState));xSemaphoreGive(mutex);
     StaticJsonDocument<640>d;d["hardware_tag"]=hardwareTag;d["code"]=c.code;d["request_id"]=c.requestId;d["name"]=Engine::cameraName();String body;serializeJson(d,body);StaticJsonDocument<640>r;int status=0;
     bool ok=request(c,"pair",(const uint8_t*)body.c_str(),body.length(),r,nullptr,&status)&&r["token"].is<const char*>()&&strlen(r["token"])==64&&r["id"].is<const char*>();
     xSemaphoreTake(mutex,portMAX_DELAY);bool current=config.revision==c.revision&&config.code==c.code;
     if(current&&ok){Config next=config;next.token=r["token"].as<String>();next.id=r["id"].as<String>();next.code="";next.requestId="";credentialsPending=!save(next);config=next;pairVerified=true;strlcpy(pairState,credentialsPending?"saving":"paired",sizeof(pairState));}
     else if(current&&PairingPolicy::permanent(status)){config.code="";config.requestId="";save(config);strlcpy(pairState,"failed",sizeof(pairState));}
     else if(current)strlcpy(pairState,"retrying",sizeof(pairState));xSemaphoreGive(mutex);
     if(current&&ok){pairing.done();nextHeartbeat=0;report(credentialsPending?"saving":"paired",credentialsPending?"Camera paired. Retrying saved settings.":"Camera connected to your account.");}
     else if(current&&!PairingPolicy::permanent(status))pairing.failed(millis());else pairing.done();c=copy();
    }
   }
   if(credentialsPending&&int32_t(millis()-saveRetry)>=0){saveRetry=millis()+60000;xSemaphoreTake(mutex,portMAX_DELAY);credentialsPending=!save(config);if(!credentialsPending)strlcpy(pairState,"paired",sizeof(pairState));xSemaphoreGive(mutex);}
   if(c.token.length()&&(changed||(int32_t)(millis()-nextHeartbeat)>=0)){
    nextHeartbeat=millis()+60000;StaticJsonDocument<1536>d;d["control_protocol"]=2;d["firmware"]="remote-cctv-v2";Live::status(d.createNestedObject("stream"));uint32_t sentNameRevision=nameRevision.load();d["name_changed"]=sentNameRevision!=nameAck;d["hardware_tag"]=hardwareTag;d["name"]=Engine::cameraName();d["managed_recipients"]=true;d["remote"]=c.remote;d["whatsapp"]=c.wa;d["photo"]=c.photo;d["phone"]=c.phone;d["opt_in"]=c.optIn;d["interval"]=c.interval;d["retention_days"]=c.retentionDays;d["timezone_minutes"]=c.timezoneMinutes;String body;serializeJson(d,body);StaticJsonDocument<2048>r;
    if(request(c,"heartbeat",(const uint8_t*)body.c_str(),body.length(),r)){needSync=false;xSemaphoreTake(mutex,portMAX_DELAY);nameAck=sentNameRevision;if(nameRevision.load()==sentNameRevision)prefs.putBool("nameSync",false);xSemaphoreGive(mutex);Live::ticket(r["live"]);String alias=r["camera_name"]|"";if(nameRevision.load()==nameAck&&alias.length()&&alias!=Engine::cameraName()){StaticJsonDocument<192>name;name["name"]=alias;String error;Engine::configure(name.as<JsonObject>(),error);}xSemaphoreTake(mutex,portMAX_DELAY);pairVerified=true;nativeServiceReady=r["live"].is<JsonObject>();providerReady=r["provider_configured"]|false;notificationsPaused=r["notifications_paused"]|false;if(config.phone.length()){config.phone="";config.optIn=false;save(config);c.phone="";c.optIn=false;}xSemaphoreGive(mutex);report(c.remote&&!r["live"].is<JsonObject>()?"service_update":on?"connected":"disabled",c.remote&&!r["live"].is<JsonObject>()?"Camera paired, but live service is not ready. Check the camera website.":c.wa&&!r["provider_configured"].as<bool>()?"Connected; Meta provider is not configured":"Service connected; delivery is reported separately");String state=r["notification"]["state"]|"disabled";xSemaphoreTake(mutex,portMAX_DELAY);strlcpy(deliveryState,state.c_str(),sizeof(deliveryState));xSemaphoreGive(mutex);Events::delivery(r["notification"]["local_event"]|0,state.c_str());String error=r["notification"]["error"]|"";if(error.length())report("connected",error.c_str());}
   }
  }
  if(on&&(c.remote||c.photo)&&configured(c)&&c.token.length()&&!image&&psramFound()){xSemaphoreTake(frameMutex,portMAX_DELAY);image=(uint8_t*)heap_caps_malloc(MAX_IMAGE,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);xSemaphoreGive(frameMutex);}
  Job j;if(xQueueReceive(queue,&j,pdMS_TO_TICKS(20))==pdTRUE){
   xSemaphoreTake(frameMutex,portMAX_DELAY);if(enabled&&!Engine::paused()&&c.token.length()){
    StaticJsonDocument<512>r;bool sent=false;for(unsigned attempt=0;attempt<3&&enabled&&!sent;attempt++){sent=request(c,"event",j.length?image:(const uint8_t*)"{}",j.length?j.length:2,r,&j);if(!sent)for(unsigned wait=0;wait<(1U<<attempt)*10&&enabled;wait++)delay(100);}
    const char*state=sent?(r["notification"]|"queued"):"upload_failed";Events::delivery(j.event,state);if(sent)report("connected","Event submitted to service; WhatsApp delivery is separate");
   }pending=false;xSemaphoreGive(frameMutex);
  }
  if(on&&c.remote&&Network::connected()){if(Live::needsTicket()&&c.token.length()&&int32_t(millis()-nextLiveTicket)>=0){nextLiveTicket=millis()+5000;StaticJsonDocument<1536>proof;if(request(c,"live",(const uint8_t*)"{}",2,proof))Live::ticket(proof.as<JsonVariant>());}Live::tick();}else Live::close();
  if(!enabled&&image&&!pending){xSemaphoreTake(frameMutex,portMAX_DELAY);free(image);image=nullptr;xSemaphoreGive(frameMutex);}delay(10);
 }
}
void begin(){nameRevision=prefs.getBool("nameSync",false)?1:0;Live::begin();mutex=xSemaphoreCreateMutex();frameMutex=xSemaphoreCreateMutex();queue=xQueueCreate(1,sizeof(Job));if(!mutex||!frameMutex||!queue)return;strlcpy(boot,randomHex(8).c_str(),sizeof(boot));String saved=prefs.getString("cloud","");if(saved.length()){DynamicJsonDocument d(6144);if(!deserializeJson(d,saved)){config.code=d["pair_code"]|"";config.requestId=d["pair_request"]|"";config.pairEpoch=d["pair_epoch"]|0;config.url=d["url"]|"";config.ca=d["ca"]|"";config.id=d["id"]|"";config.token=d["token"]|"";config.phone=d["phone"]|"";config.remote=d["remote"]|false;config.wa=d["whatsapp"]|false;config.photo=d["photo"]|true;config.optIn=d["opt_in"]|false;config.interval=d["interval"]|60;config.retentionDays=d["retention_days"]|7;config.timezoneMinutes=d["timezone_minutes"]|0;}}if(config.url.length()&&config.url!=ServiceOrigin::url&&(config.url==ROBETIX_LEGACY_ORIGIN||config.url==ROBETIX_CUSTOMER_ORIGIN||(config.url.startsWith("https://robetixvision-")&&config.url.endsWith("-robetix-s-projects.vercel.app")))){config.url=ServiceOrigin::url;config.ca="";credentialsPending=!save(config);}enabled=config.remote||config.wa;if(xTaskCreate(task,"cloud",12288,nullptr,1,nullptr)!=pdPASS){vQueueDelete(queue);queue=nullptr;enabled=false;report("unavailable","Cloud worker could not start");}}
bool test(String&err){if(!mutex||!queue){err="Cloud worker unavailable";return false;}Config c=copy();if(!enabled||!c.wa||c.token.isEmpty()){err="Enable and pair WhatsApp first";return false;}if(Engine::paused()||!Camera::ready){err="Resume the camera first";return false;}if(pending||testPending.exchange(true)){err="A notification is already pending";return false;}return true;}
void halt(){enabled=false;testPending=false;dirty=true;if(mutex){xSemaphoreTake(mutex,portMAX_DELAY);config=Config{};xSemaphoreGive(mutex);}}
bool prepareReset(){if(!queue||!mutex)return false;resetDone=false;resetVerified=false;resetRequested=true;uint32_t start=millis();while(!resetDone&&millis()-start<20000)delay(25);return resetDone&&resetVerified;}
void nameChanged(){if(!mutex)return;xSemaphoreTake(mutex,portMAX_DELAY);prefs.putBool("nameSync",true);nameRevision++;xSemaphoreGive(mutex);dirty=true;}
bool wantsTest(){return testPending.load();}
void offer(camera_fb_t*f,uint32_t event,uint32_t epoch,const char*,uint8_t score,int){
 bool testing=testPending.exchange(false);if(!enabled||(!event&&!testing))return;Config c=copy();if(!c.token.length())return;bool include=c.remote||c.photo;
 if(pending||(include&&f->len>MAX_IMAGE)||xSemaphoreTake(frameMutex,0)!=pdTRUE){xSemaphoreTake(mutex,portMAX_DELAY);dropped++;xSemaphoreGive(mutex);Events::delivery(event,"busy_skipped");return;}
 if(include&&!image){xSemaphoreGive(frameMutex);Events::delivery(event,"busy_skipped");return;}Job j;j.event=event;j.epoch=epoch;j.score=score;j.offset=c.timezoneMinutes;j.test=testing;j.length=include?f->len:0;if(j.length)memcpy(image,f->buf,j.length);if(testing)snprintf(j.key,sizeof(j.key),"test_%s_%lu",boot,(unsigned long)++testCounter);else snprintf(j.key,sizeof(j.key),"event_%lu",(unsigned long)event);
 pending=true;if(xQueueSend(queue,&j,0)!=pdTRUE)pending=false;xSemaphoreGive(frameMutex);Events::delivery(event,pending?"queued":"busy_skipped");
}
void status(JsonObject o){o["customer_service_url"]=ServiceOrigin::customer;o["default_service_url"]=ServiceOrigin::url;o["build_mode"]=ServiceOrigin::mode;if(!mutex){o["state"]="unavailable";return;}xSemaphoreTake(mutex,portMAX_DELAY);o["paired"]=pairVerified;o["remote_service_ready"]=nativeServiceReady;o["service_matches_build"]=config.url==ServiceOrigin::url||nativeServiceReady;o["pair_state"]=pairState;o["operation_id"]=config.revision;o["settings_saved"]=!credentialsPending;o["http_status"]=lastHTTP.load();o["state"]=!config.remote&&!config.wa&&config.token.length()?"disabled":phase;o["message"]=detail;o["configured"]=config.token.length()>0;o["remote"]=config.remote;o["whatsapp"]=config.wa;o["photo"]=config.photo;o["managed_recipients"]=true;o["device_id"]=config.id;o["provider_configured"]=providerReady;o["notifications_paused"]=notificationsPaused;o["interval"]=config.interval;o["retention_days"]=config.retentionDays;o["timezone_minutes"]=config.timezoneMinutes;o["url"]=config.url;o["ca_configured"]=true;Live::status(o.createNestedObject("live"));o["dropped"]=dropped;o["last_success_uptime"]=lastSuccess;o["notification"]=deliveryState;xSemaphoreGive(mutex);}
}
