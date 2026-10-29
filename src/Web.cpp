#include "Web.h"
#include "DeviceStatus.h"
#include "Settings.h"
#include "Camera.h"
#include "Network.h"
#include "Storage.h"
#include "Ui.h"
#include "Engine.h"
#include "Auth.h"
#include "Events.h"
#include "Motion.h"
#include "Cloud.h"
#include "Light.h"
#include <esp_system.h>
#include "Maintenance.h"
#include <esp_heap_caps.h>
#include <esp_http_server.h>
#include <esp_timer.h>
#include <WiFi.h>
#include <lwip/sockets.h>
#include <esp_wifi.h>
#include <esp_netif_sta_list.h>
#include <atomic>
static httpd_handle_t controlServer,streamServer;
static std::atomic<uint32_t> streamFrames{0},streamMs{0};
static std::atomic<bool> streaming{false};
static uint32_t previewRateAt=0,previewDelivered=0,lastPreviewAt=0,lastPreviewId=0;static float previewRate=0;
static uint8_t *controlFrame=nullptr,*mediaFrame=nullptr;
static esp_err_t reply(httpd_req_t*,const char*,const char*);
static bool query(httpd_req_t*r,const char*key,char*out,size_t size){char q[192];return httpd_req_get_url_query_str(r,q,sizeof(q))==ESP_OK&&httpd_query_key_value(q,key,out,size)==ESP_OK;}
static esp_err_t cameraFrame(httpd_req_t*r,uint8_t*buffer,bool download){
 Engine::requestPreview();size_t length=0;uint32_t id=0,age=0;char after[16]={};query(r,"after",after,sizeof(after));
 if(!Engine::copyFrame(buffer,Engine::frameCapacity(),length,id,age,*after?strtoul(after,nullptr,10):0))return reply(r,"{\"error\":\"Waiting for a fresh frame\"}","503 Service Unavailable");

 char seq[16];snprintf(seq,sizeof(seq),"%lu",(unsigned long)id);httpd_resp_set_hdr(r,"X-Frame-Id",seq);httpd_resp_set_hdr(r,"Cache-Control","no-store");httpd_resp_set_type(r,"image/jpeg");
 if(download)httpd_resp_set_hdr(r,"Content-Disposition","attachment; filename=robetix.jpg");esp_err_t result=httpd_resp_send(r,(const char*)buffer,length);if(result==ESP_OK&&!download&&id!=lastPreviewId){uint32_t now=millis();if(!lastPreviewAt||now-lastPreviewAt>2000){previewRateAt=now;previewDelivered=0;previewRate=0;}lastPreviewAt=now;lastPreviewId=id;previewDelivered++;if(now-previewRateAt>=1000){previewRate=previewDelivered*1000.0/(now-previewRateAt);previewRateAt=now;previewDelivered=0;}}return result;
}
static esp_err_t reply(httpd_req_t *r,const char *body,const char *status="200 OK") {
  httpd_resp_set_status(r,status); httpd_resp_set_type(r,"application/json"); httpd_resp_set_hdr(r,"Cache-Control","no-store"); return httpd_resp_send(r,body,HTTPD_RESP_USE_STRLEN);
}
static esp_err_t json(httpd_req_t*r,JsonDocument &d) {String out;serializeJson(d,out);return reply(r,out.c_str());}
static bool authorized(httpd_req_t*r,String *token=nullptr) {
  char cookie[160]={}; if(httpd_req_get_hdr_value_str(r,"Cookie",cookie,sizeof(cookie))!=ESP_OK)return false;
  char *save=nullptr;for(char*p=strtok_r(cookie,";",&save);p;p=strtok_r(nullptr,";",&save)){while(*p==' ')p++;if(strncmp(p,"rv=",3)==0&&Auth::valid(p+3)){if(token)*token=p+3;return true;}}return false;
}
// Require both the AP interface and an actual associated station. A router-side
// request, forged Host header or client IP by itself cannot establish trust.
static bool cameraWifi(httpd_req_t*r){
 sockaddr_in local={},peer={};socklen_t n=sizeof(local);int fd=httpd_req_to_sockfd(r);
 if(getsockname(fd,(sockaddr*)&local,&n)||IPAddress(local.sin_addr.s_addr)!=WiFi.softAPIP())return false;
 n=sizeof(peer);if(getpeername(fd,(sockaddr*)&peer,&n))return false;
 wifi_sta_list_t stations={};esp_netif_sta_list_t addresses={};
 if(esp_wifi_ap_get_sta_list(&stations)!=ESP_OK||esp_netif_get_sta_list(&stations,&addresses)!=ESP_OK)return false;
 for(int i=0;i<addresses.num;i++)if(addresses.sta[i].ip.addr==peer.sin_addr.s_addr)return true;
 return false;
}
static bool sameCameraOrigin(httpd_req_t*r){
 char host[80]={},origin[96]={};if(httpd_req_get_hdr_value_str(r,"Host",host,sizeof(host))!=ESP_OK)return false;
 String h=host;bool known=h=="192.168.4.1"||h==WiFi.localIP().toString()||h=="robetix.local";
 if(!known)return false;
 return httpd_req_get_hdr_value_str(r,"Origin",origin,sizeof(origin))==ESP_OK&&String(origin)=="http://"+h;
}
static bool body(httpd_req_t*r,JsonDocument&d) {
  if(r->content_len<=0 || r->content_len>768) return false;
  char b[769];int offset=0;
  while(offset<r->content_len){int n=httpd_req_recv(r,b+offset,r->content_len-offset);if(n<=0)return false;offset+=n;}
  b[offset]=0;return !deserializeJson(d,b);
}
static bool localPost(httpd_req_t*r) {
  char type[64]={};httpd_req_get_hdr_value_str(r,"Content-Type",type,sizeof(type));
  // JSON-only mutation APIs reject cross-origin simple requests; no CORS permissions are issued.
  return strncmp(type,"application/json",16)==0;
}
static esp_err_t handler(httpd_req_t*r) {
  String uri=r->uri;int question=uri.indexOf('?');if(question>=0)uri=uri.substring(0,question);
  if(r->method==HTTP_GET&&(uri=="/generate_204"||uri=="/hotspot-detect.html"||uri=="/connecttest.txt"||uri=="/ncsi.txt")){httpd_resp_set_status(r,"302 Found");httpd_resp_set_hdr(r,"Location","http://192.168.4.1/");return httpd_resp_send(r,"",0);}
  if(r->method==HTTP_GET && uri=="/") {httpd_resp_set_type(r,"text/html");httpd_resp_set_hdr(r,"Cache-Control","no-store");httpd_resp_set_hdr(r,"X-Frame-Options","DENY");return httpd_resp_send(r,UI,HTTPD_RESP_USE_STRLEN);}
  if(r->method==HTTP_GET && uri=="/api/session") {StaticJsonDocument<192>d;d["device"]=passwordHash("public-device-id").substring(0,16);d["setup"]=false;d["password_enabled"]=false;d["authenticated"]=authorized(r);return json(r,d);}
  if(r->method==HTTP_POST && !localPost(r)) return reply(r,"{\"error\":\"JSON required\"}","415 Unsupported Media Type");
  if(r->method==HTTP_POST&&(uri=="/api/login"||uri=="/api/provision"||uri=="/api/trust"||uri=="/api/recover"||uri=="/api/authorize"))return reply(r,"{\"error\":\"Connect to the camera Wi-Fi to authorize this browser.\"}","410 Gone");
  if(r->method==HTTP_POST&&uri=="/api/hotspot-session"){
    if(!cameraWifi(r)||!sameCameraOrigin(r))return reply(r,"{\"error\":\"Connect to the camera Wi-Fi to authorize this browser.\"}","403 Forbidden");
    if(!authorized(r)){String token=Auth::issue(true);if(token.isEmpty())return reply(r,"{\"error\":\"Could not remember this device\"}","503 Service Unavailable");String cookie="rv="+token+"; HttpOnly; SameSite=Strict; Path=/; Max-Age=31536000";httpd_resp_set_hdr(r,"Set-Cookie",cookie.c_str());}
    return reply(r,"{}");
  }
  if(r->method==HTTP_POST&&uri=="/api/accept-handoff"){StaticJsonDocument<128>d;if(!body(r,d))return reply(r,"{\"error\":\"Invalid request\"}","400 Bad Request");bool remember=false;String token=Auth::acceptHandoff(d["code"]|"",remember);if(token.isEmpty())return reply(r,"{\"error\":\"Local access link expired. Open the camera from your trusted page again.\"}","403 Forbidden");String cookie="rv="+token+"; HttpOnly; SameSite=Strict; Path=/";if(remember)cookie+="; Max-Age=31536000";httpd_resp_set_hdr(r,"Set-Cookie",cookie.c_str());return reply(r,"{}");}
  if(!authorized(r))return reply(r,"{\"error\":\"Sign in required\"}","401 Unauthorized");
  if(r->method==HTTP_POST&&uri=="/api/handoff"){String token;authorized(r,&token);String code=Auth::handoff(token);if(code.isEmpty()||!Network::connected())return reply(r,"{\"error\":\"Connect to home Wi-Fi first\"}","409 Conflict");StaticJsonDocument<192>d;d["code"]=code;d["ip"]=WiFi.localIP().toString();d["expires_in"]=60;return json(r,d);}

  if(r->method==HTTP_POST&&uri=="/api/setup"){StaticJsonDocument<64>d;if(!body(r,d)||d["complete"]!=true)return reply(r,"{\"error\":\"Invalid request\"}","400 Bad Request");return prefs.putBool("setupDone",true)?reply(r,"{}"):reply(r,"{\"error\":\"Settings write failed\"}","500 Internal Server Error");}
  if(r->method==HTTP_POST && uri=="/api/logout") {String token;authorized(r,&token);Auth::revoke(token);httpd_resp_set_hdr(r,"Set-Cookie","rv=; Path=/; Max-Age=0");return reply(r,"{}");}
  if(r->method==HTTP_GET && uri=="/api/status") {
    StaticJsonDocument<6144>d;
    DeviceStatus::write(d.to<JsonObject>());d["preview_fps"]=millis()-lastPreviewAt<2000?previewRate:0;d["streaming"]=streaming.load();d["stream_fps"]=streamMs.load()?streamFrames.load()*1000.0/streamMs.load():0;return json(r,d);
  }
  if(r->method==HTTP_GET && uri=="/api/benchmark") {StaticJsonDocument<4096>d;Camera::benchmarkStatus(d.to<JsonObject>());return json(r,d);}
  if(r->method==HTTP_POST && uri=="/api/benchmark") {Engine::Command c;c.action=Engine::Action::Benchmark;return Engine::submit(c)?reply(r,"{}","202 Accepted"):reply(r,"{\"error\":\"Camera busy\"}","409 Conflict");}
  if(r->method==HTTP_GET&&(uri=="/api/preview"||uri=="/api/snapshot"))return cameraFrame(r,controlFrame,uri=="/api/snapshot");
  if(r->method==HTTP_GET&&uri=="/api/cloud"){StaticJsonDocument<2048>d;Cloud::status(d.to<JsonObject>());return json(r,d);}
  if(r->method==HTTP_POST&&uri=="/api/cloud/test"){String err;if(!Cloud::test(err)){StaticJsonDocument<256>d;d["error"]=err;String s;serializeJson(d,s);return reply(r,s.c_str(),"409 Conflict");}return reply(r,"{}","202 Accepted");}
  if(r->method==HTTP_POST&&uri=="/api/cloud"){if(r->content_len<=0||r->content_len>6144)return reply(r,"{\"error\":\"Cloud configuration is too large\"}","400 Bad Request");String input;input.reserve(r->content_len);char chunk[256];int remaining=r->content_len;while(remaining){int n=httpd_req_recv(r,chunk,min(remaining,255));if(n<=0)return ESP_FAIL;chunk[n]=0;input+=chunk;remaining-=n;}DynamicJsonDocument d(7168);String error;if(deserializeJson(d,input)||!Cloud::configure(d.as<JsonObject>(),error)){StaticJsonDocument<256>out;out["error"]=error.length()?error:"Invalid configuration";String s;serializeJson(out,s);return reply(r,s.c_str(),"400 Bad Request");}return reply(r,"{}","202 Accepted");}
  if(r->method==HTTP_POST&&uri=="/api/light/settings"){StaticJsonDocument<128>d;if(!body(r,d)||!d["preferred"].is<int>()||!d["auto_off"].is<int>()||d["preferred"].as<int>()<0||d["preferred"].as<int>()>100||d["auto_off"].as<int>()<10||d["auto_off"].as<int>()>300||!Light::configure(d["preferred"],d["auto_off"]))return reply(r,"{\"error\":\"Invalid light settings\"}","400 Bad Request");return reply(r,"{}");}
  if(r->method==HTTP_POST&&uri=="/api/motion/areas"){StaticJsonDocument<64>d;if(!body(r,d))return reply(r,"{\"error\":\"Invalid movement display request\"}","400 Bad Request");Motion::showAreas(d["enabled"]|false);return reply(r,"{}");}
  if(r->method==HTTP_POST&&uri=="/api/motion/test"){if(!Motion::available())return reply(r,"{\"error\":\"Motion analysis needs available PSRAM\"}","503 Service Unavailable");StaticJsonDocument<64>d;if(r->content_len&&!body(r,d))return reply(r,"{\"error\":\"Invalid motion test request\"}","400 Bad Request");Motion::calibrate(d["enabled"]|true);return reply(r,"{}");}
  if(r->method==HTTP_GET&&uri=="/api/events"){StaticJsonDocument<6144>d;Events::list(d.to<JsonObject>());return json(r,d);}
  if(r->method==HTTP_GET&&uri=="/api/cleanup"){StaticJsonDocument<1536>d;Maintenance::status(d.to<JsonObject>());return json(r,d);}
  if(r->method==HTTP_POST&&uri=="/api/cleanup"){
    StaticJsonDocument<512>d;String error;if(!body(r,d))return reply(r,"{\"error\":\"Invalid storage request\"}","400 Bad Request");
    String action=d["action"]|"";bool ok=false;
    if(action=="find"&&d["from"].is<uint32_t>()&&d["to"].is<uint32_t>())ok=Maintenance::find(d["kind"]|"",d["from"],d["to"],error);
    else if(action=="delete"&&d["confirm"].as<bool>())ok=Maintenance::confirm(d["token"]|"",error);
    if(!ok){StaticJsonDocument<256>e;e["error"]=error.length()?error:"Invalid storage request";httpd_resp_set_status(r,"409 Conflict");String s;serializeJson(e,s);return reply(r,s.c_str(),"409 Conflict");}return reply(r,"{}","202 Accepted");
  }
  if(r->method==HTTP_POST&&uri=="/api/action"){
    StaticJsonDocument<256>d;if(!body(r,d))return reply(r,"{\"error\":\"Invalid action\"}","400 Bad Request");
    String action=d["action"]|"";Engine::Command c;
    if(action=="record")c.action=Engine::Action::Start;else if(action=="stop")c.action=Engine::Action::Stop;else if(action=="photo")c.action=Engine::Action::Photo;else if(action=="restart")c.action=Engine::Action::Restart;
    else if(action=="light"&&d["brightness"].is<int>()&&d["brightness"].as<int>()>=0&&d["brightness"].as<int>()<=100){c.action=Engine::Action::Light;c.camera[0]=d["brightness"];}
    else if(action=="reset_camera")c.action=Engine::Action::ResetCamera;
    else if(action=="factory_reset"&&String(d["confirmation"]|"")=="ERASE SETTINGS"){if(!cameraWifi(r)||!sameCameraOrigin(r))return reply(r,"{\"error\":\"Connect to the camera Wi-Fi before factory reset.\"}","403 Forbidden");c.action=Engine::Action::FactoryReset;}
    else if(action=="privacy"&&d["enabled"].is<bool>()){c.action=Engine::Action::Privacy;c.flag=d["enabled"];}else return reply(r,"{\"error\":\"Unknown action\"}","400 Bad Request");
    return Engine::submit(c)?reply(r,"{}","202 Accepted"):reply(r,"{\"error\":\"Camera busy; retry\"}","409 Conflict");
  }
  if(r->method==HTTP_POST&&uri=="/api/options"){StaticJsonDocument<1024>d;String err;if(!body(r,d)||!Engine::configure(d.as<JsonObject>(),err))return reply(r,"{\"error\":\"Invalid settings or camera busy\"}","400 Bad Request");return reply(r,"{}","202 Accepted");}
  if(r->method==HTTP_POST&&uri=="/api/time"){StaticJsonDocument<128>d;if(!body(r,d)||!d["epoch"].is<uint32_t>()||!Engine::setTime(d["epoch"]))return reply(r,"{\"error\":\"Invalid time\"}","400 Bad Request");return reply(r,"{}");}
  if(r->method==HTTP_GET&&uri=="/api/files"){
    char kind[12]={},before[16]={},from[16]={},to[16]={};if(!query(r,"kind",kind,sizeof(kind)))return reply(r,"{\"error\":\"Choose photos or videos\"}","400 Bad Request");query(r,"before",before,sizeof(before));query(r,"from",from,sizeof(from));query(r,"to",to,sizeof(to));
    if(!Storage::available||xSemaphoreTake(Storage::operationMutex,0)!=pdTRUE)return reply(r,"{\"error\":\"Stop recording or wait for the card operation to finish\"}","409 Conflict");
    StaticJsonDocument<4096>d;bool ok=Storage::list(kind,*before?strtoul(before,nullptr,10):UINT32_MAX,d.to<JsonObject>(),*from?strtoul(from,nullptr,10):0,*to?strtoul(to,nullptr,10):UINT32_MAX);xSemaphoreGive(Storage::operationMutex);return ok?json(r,d):reply(r,"{\"error\":\"Cannot read saved files\"}","400 Bad Request");
  }
  if(r->method==HTTP_POST&&uri=="/api/delete"){
    StaticJsonDocument<256>d;if(!body(r,d)||!Storage::validName(d["name"]|"",d["kind"]|""))return reply(r,"{\"error\":\"Invalid file\"}","400 Bad Request");
    if(!Storage::available||xSemaphoreTake(Storage::operationMutex,0)!=pdTRUE)return reply(r,"{\"error\":\"Memory card busy\"}","409 Conflict");bool ok=Storage::remove(d["name"],d["kind"]);xSemaphoreGive(Storage::operationMutex);return ok?reply(r,"{}"):reply(r,"{\"error\":\"Could not delete file\"}","500 Internal Server Error");
  }
  if(r->method==HTTP_GET && uri=="/api/scan") {
    int n=WiFi.scanComplete();StaticJsonDocument<2048>d;auto a=d.createNestedArray("networks");
    if(n==WIFI_SCAN_FAILED){WiFi.scanNetworks(true);d["running"]=true;}else if(n==WIFI_SCAN_RUNNING)d["running"]=true;
    else {for(int i=0;i<min(n,16);i++){auto x=a.createNestedObject();x["ssid"]=WiFi.SSID(i);x["rssi"]=WiFi.RSSI(i);}WiFi.scanDelete();d["running"]=false;}
    return json(r,d);
  }
  if(r->method==HTTP_POST && uri=="/api/network") {
    if(Engine::occupied())return reply(r,"{\"error\":\"Stop recording before changing Wi-Fi\"}","409 Conflict");
    StaticJsonDocument<1024>d;if(!body(r,d))return reply(r,"{\"error\":\"Invalid network settings\"}","400 Bad Request");
    String error;bool changed=false;if(!Network::configure(d.as<JsonObject>(),error,changed)){StaticJsonDocument<256>out;out["error"]=error;String result;serializeJson(out,result);return reply(r,result.c_str(),"400 Bad Request");}
    esp_err_t result=reply(r,changed?"{\"apply_delay_seconds\":2}":"{\"apply_delay_seconds\":0}");if(changed)Network::changed();return result;
  }

  if(r->method==HTTP_POST&&(uri=="/api/camera"||uri=="/api/storage/snapshot")){
    Engine::Command c;c.action=uri=="/api/camera"?Engine::Action::Camera:Engine::Action::Photo;
    if(c.action==Engine::Action::Camera){StaticJsonDocument<768>d;if(!body(r,d))return reply(r,"{\"error\":\"Invalid settings\"}","400 Bad Request");int i=0;for(const char*k:{"size","quality","brightness","contrast","saturation","mirror","flip"}){if(!d[k].is<int>())return reply(r,"{\"error\":\"Missing camera setting\"}","400 Bad Request");c.camera[i++]=d[k];}c.camera[7]=d["white_balance"]|prefs.getInt("wb",0);c.camera[8]=d["exposure"]|prefs.getInt("exposure",-1);c.camera[9]=d["gain"]|prefs.getInt("gain",-1);}
    return Engine::submit(c)?reply(r,"{}","202 Accepted"):reply(r,"{\"error\":\"Camera busy\"}","409 Conflict");
  }
  return reply(r,"{\"error\":\"Not found\"}","404 Not Found");
}
static esp_err_t stream(httpd_req_t*r) {
  String streamSession;
  if(!authorized(r,&streamSession))return reply(r,"{\"error\":\"Sign in on port 80 first\"}","401 Unauthorized");
  // HTTPD purges request headers after sending the response. Keep the authorized
  // token locally and check revocation without rereading the Cookie header.
  if(Camera::busy()||!Camera::ready)return reply(r,"{\"error\":\"Camera unavailable\"}","503 Service Unavailable");
  streaming=true;streamFrames=0;uint32_t start=millis();esp_err_t err=ESP_OK;uint32_t lastId=0,lastFresh=millis();
  httpd_resp_set_type(r,"multipart/x-mixed-replace;boundary=rvframe");httpd_resp_set_hdr(r,"Cache-Control","no-store");
  while(!Camera::busy()&&!Engine::paused()) {
    if(!Auth::valid(streamSession))break;
    uint32_t frameStart=millis();
    Engine::requestPreview();size_t length=0;uint32_t id=0,age=0;
    if(!Engine::copyFrame(mediaFrame,Engine::frameCapacity(),length,id,age,lastId)){vTaskDelay(pdMS_TO_TICKS(100));if(millis()-lastFresh>5000)break;continue;}
    lastId=id;lastFresh=millis();
    char head[128];int n=snprintf(head,sizeof(head),"--rvframe\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",unsigned(length));
    err=httpd_resp_send_chunk(r,head,n);if(err==ESP_OK)err=httpd_resp_send_chunk(r,(const char*)mediaFrame,length);if(err==ESP_OK)err=httpd_resp_send_chunk(r,"\r\n",2);
    if(err!=ESP_OK)break;streamFrames++;streamMs=millis()-start;
    uint32_t elapsed=millis()-frameStart;vTaskDelay(pdMS_TO_TICKS(elapsed<100?100-elapsed:1));
  }
  streaming=false;return err==ESP_OK?httpd_resp_send_chunk(r,nullptr,0):err;
}
static esp_err_t media(httpd_req_t*r){
 String uri=r->uri;if(uri.startsWith("/stream"))return stream(r);
 if(!authorized(r))return reply(r,"{\"error\":\"Sign in first\"}","401 Unauthorized");
 char name[40]={},kind[12]={},download[4]={};query(r,"download",download,sizeof(download));
 if(!query(r,"name",name,sizeof(name))||!query(r,"kind",kind,sizeof(kind))||!Storage::validName(name,kind))return reply(r,"{\"error\":\"Invalid file\"}","400 Bad Request");
 if(!Storage::available||xSemaphoreTake(Storage::operationMutex,0)!=pdTRUE)return reply(r,"{\"error\":\"Stop recording before opening saved files\"}","409 Conflict");
 File f=SD_MMC.open(Storage::path(name,kind),FILE_READ);if(!f){xSemaphoreGive(Storage::operationMutex);return reply(r,"{\"error\":\"File not found\"}","404 Not Found");}
 bool video=String(kind)=="videos";httpd_resp_set_type(r,video?"video/x-msvideo":"image/jpeg");String disposition=String(video||*download=='1'?"attachment; filename=\"":"inline; filename=\"")+name+"\"";httpd_resp_set_hdr(r,"Content-Disposition",disposition.c_str());httpd_resp_set_hdr(r,"Cache-Control","no-store");
 uint8_t buffer[1024];esp_err_t e=ESP_OK;size_t remaining=f.size();while(remaining&&e==ESP_OK){size_t n=f.read(buffer,min(remaining,sizeof(buffer)));if(!n){e=ESP_FAIL;break;}e=httpd_resp_send_chunk(r,(char*)buffer,n);remaining-=n;delay(1);}f.close();xSemaphoreGive(Storage::operationMutex);return e==ESP_OK?httpd_resp_send_chunk(r,nullptr,0):e;
}
void webBegin() {
  Auth::begin();
  size_t size=Engine::frameCapacity();if(size){controlFrame=(uint8_t*)heap_caps_malloc(size,psramFound()?MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT:MALLOC_CAP_8BIT);mediaFrame=(uint8_t*)heap_caps_malloc(size,psramFound()?MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT:MALLOC_CAP_8BIT);}
  httpd_config_t config=HTTPD_DEFAULT_CONFIG();config.server_port=80;config.ctrl_port=32768;config.max_uri_handlers=3;config.max_open_sockets=3;config.lru_purge_enable=true;config.stack_size=12288;config.recv_wait_timeout=3;config.send_wait_timeout=2;config.uri_match_fn=httpd_uri_match_wildcard;
  if(httpd_start(&controlServer,&config)==ESP_OK){httpd_uri_t get={};get.uri="/*";get.method=HTTP_GET;get.handler=handler;httpd_register_uri_handler(controlServer,&get);get.method=HTTP_POST;httpd_register_uri_handler(controlServer,&get);}else Serial.println("Control HTTP start failed");
  config.server_port=81;config.ctrl_port=32769;config.max_open_sockets=1;config.lru_purge_enable=false;config.stack_size=4096;
  if(httpd_start(&streamServer,&config)==ESP_OK){httpd_uri_t u={};u.uri="/*";u.method=HTTP_GET;u.handler=media;httpd_register_uri_handler(streamServer,&u);}else Serial.println("Media HTTP start failed");
}




