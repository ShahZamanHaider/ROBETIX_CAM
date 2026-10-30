#include "Live.h"
#include "TlsRoots.h"
#include "TlsMemory.h"
#include "VerifiedTLS.h"
#include "Engine.h"
#include "RemoteControl.h"
#include "LiveLease.h"
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <mbedtls/sha1.h>
#include <mbedtls/base64.h>
#include <atomic>

namespace Live {
static VerifiedTLS client;
static char lastHost[64] = {};
static SemaphoreHandle_t lock;
static uint8_t *frame;
static size_t length;
static uint16_t width, height;
static uint64_t captured;
static std::atomic<bool> demand{false};
static std::atomic<uint32_t> due{0};
static std::atomic<uint8_t> quality{0};
static std::atomic<uint8_t> blocked{0};
static uint32_t lastStatus=0;
static LiveLease lease;
static uint32_t clockAt=0, connectAttempts=0, acked=0, lastAck=0;
static uint64_t serverClock=0;
static int handshakeCode=0;
static char lastError[80] = {};
static String url, proof;
static uint32_t expires=0, nextTry=0, seq=0, sent=0, dropped=0;
static uint32_t lastPing=0, lastSendUs=0, maxSendUs=0, sentBytes=0;
static char state[28] = "unavailable";
// Worker accepts each ticket once and closes the old socket at its own expiry.
// Refreshing a *ticket* must not interrupt the socket that is already streaming.
static uint32_t socketUntil=0;
static bool freshTicket=false;

static bool writeAll(const uint8_t *p, size_t n) {
  uint32_t start=millis();
  while(n && millis()-start<2000) {
    size_t k=client.write(p,min(n,size_t(1024)));
    if(!k)return false;
    p+=k;n-=k;delay(1);
  }
  return !n;
}

static bool send(uint8_t opcode,const uint8_t *p,size_t n) {
  uint32_t deadline=millis()+3000;
  uint8_t h[14],mask[4];esp_fill_random(mask,4);
  h[0]=0x80|opcode;
  size_t hn=2;
  if(n<126)h[1]=0x80|n;
  else {
    h[1]=0x80|126;h[2]=n>>8;h[3]=n;hn=4;
    if(n>65535){
      h[1]=0x80|127;memset(h+2,0,8);
      uint64_t nn=n;
      for(int i=9;i>=2;i--){h[i]=nn;nn>>=8;}
      hn=10;
    }
  }
  memcpy(h+hn,mask,4);hn+=4;
  if(!writeAll(h,hn))return false;
  uint8_t chunk[1024];size_t at=0;
  while(at<n){
    if((int32_t)(millis()-deadline)>=0)return false;
    size_t k=min(sizeof(chunk),n-at);
    for(size_t i=0;i<k;i++)chunk[i]=p[at+i]^mask[(at+i)%4];
    if(!writeAll(chunk,k))return false;
    at+=k;
  }
  return true;
}

bool sendPacket(const uint8_t*p,size_t n){return n<=8192+32&&send(2,p,n);}

// A newly issued ticket is needed before a reconnect (tickets are one-time use).
// Request a spare ticket before an established socket reaches its 90s expiry.
bool needsTicket(){
  if(lease.required)return true;
  if(!client.connected())return !freshTicket;
  return !freshTicket && (int32_t)(socketUntil-millis())<=30000;
}

void close(){
  lease.invalidate();
  freshTicket=false;socketUntil=0;
  proof="";demand=false;
  client.stop();
  if(lock && xSemaphoreTake(lock,pdMS_TO_TICKS(20))==pdTRUE){
    length=0;free(frame);frame=nullptr;xSemaphoreGive(lock);
  }
  strlcpy(state,"reconnecting",sizeof(state));
}

void begin(){lock=xSemaphoreCreateMutex();strlcpy(state,"waiting_for_service",sizeof(state));}

void ticket(JsonVariantConst v){
  String incomingUrl=v["relay"]|"";
  String incomingProof=v["ticket"]|"";
  uint32_t incomingExpires=v["expires"]|0;
  if(!incomingUrl.startsWith("wss://") || incomingProof.isEmpty() || !incomingExpires)return;
  uint64_t now=v["server_ms"]|uint64_t(Engine::epoch())*1000;
  // Do not kill the active camera WebSocket simply to store a new ticket.
  bool keep=client.connected() && incomingUrl==url;
  if(!keep)close();
  url=incomingUrl;proof=incomingProof;expires=incomingExpires;
  serverClock=now;clockAt=millis();
  lease.accept(now,expires,millis());
  freshTicket=true;
  if(!keep)nextTry=0;
}

static String b64(const uint8_t *p,size_t n){
  unsigned char out[64];size_t len=0;
  mbedtls_base64_encode(out,sizeof(out),&len,p,n);
  return String((char*)out,len);
}

static bool connect(){
  connectAttempts++;handshakeCode=0;
  if(!freshTicket || !url.startsWith("wss://") || proof.length()>1400 || proof.isEmpty() || !lease.valid(millis(),3000))return false;
  String target=url.substring(6);
  int slash=target.indexOf('/');if(slash<1)return false;
  String host=target.substring(0,slash),path=target.substring(slash);
  for(auto c:host)if(!isalnum((unsigned char)c)&&c!='.'&&c!='-')return false;
  strlcpy(lastHost,host.c_str(),sizeof(lastHost));
  if(!TlsMemory::enough(ESP.getFreeHeap(),heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL))){
    strlcpy(state,"memory_limited",sizeof(state));
    strlcpy(lastError,"Insufficient internal heap for TLS",sizeof(lastError));
    return false;
  }
  client.setCACert(ROBETIX_SERVICE_ROOTS);
  // Preserve the user's tested, working TLS timeouts.
  client.setHandshakeTimeout(12);
  client.setTimeout(8);
  if(!client.connect(host.c_str(),443,10000)){
    snprintf(lastError,sizeof(lastError),"TLS failed r=%d v=%lu",client.result,(unsigned long)client.verification);
    Serial.printf("ROBETIX_LIVE_TLS: host=%s result=%d verify=%lu\n",host.c_str(),client.result,(unsigned long)client.verification);
    return false;
  }
  uint8_t nonce[16];esp_fill_random(nonce,16);
  String key=b64(nonce,16),expected=key+"258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  uint8_t digest[20];
  mbedtls_sha1_ret((const uint8_t*)expected.c_str(),expected.length(),digest);
  expected=b64(digest,20);
  String req="GET "+path+" HTTP/1.1\r\nHost: "+host+
    "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: "+key+
    "\r\nSec-WebSocket-Protocol: robetix.live.v1, ticket."+proof+"\r\n\r\n";
  if(!writeAll((const uint8_t*)req.c_str(),req.length()))return false;
  String headers;uint32_t start=millis();
  while(millis()-start<3000 && headers.length()<4096){
    if(client.available()){
      headers+=(char)client.read();
      if(headers.endsWith("\r\n\r\n"))break;
    }else delay(2);
  }
  handshakeCode=headers.substring(9,12).toInt();
  if(!headers.startsWith("HTTP/1.1 101 ") || headers.indexOf(expected)<0 || headers.indexOf("robetix.live.v1")<0)return false;
  strlcpy(state,"connected",sizeof(state));
  lastError[0]=0;
  lastPing=millis();
  socketUntil=lease.until;
  // Connection tickets are single-use on the relay.
  freshTicket=false;
  proof="";
  return true;
}

static bool readExact(uint8_t *p,size_t n){
  uint32_t start=millis();
  while(n && millis()-start<1000){
    int k=client.read(p,n);
    if(k>0){p+=k;n-=k;}
    else {if(!client.connected())return false;delay(1);}
  }
  return !n;
}

bool viewing(){return demand&&!Engine::paused();}
uint8_t profile(){return quality.load();}
bool wanted(){return demand&&!Engine::paused()&&(int32_t)(millis()-due.load())>=0;}

void offer(camera_fb_t*f,bool busy){
  if(!wanted())return;
  due=millis()+(quality==0?1000:500);
  if(busy||!lock||f->len>131072||xSemaphoreTake(lock,0)!=pdTRUE){
    blocked=busy?1:f->len>131072?2:0;dropped++;return;
  }
  blocked=0;
  if(!frame||length){dropped++;xSemaphoreGive(lock);return;}
  memcpy(frame+24,f->buf,f->len);
  length=f->len+24;width=f->width;height=f->height;captured=millis();
  xSemaphoreGive(lock);
}

void tick(){
  if(!lock||url.isEmpty())return;
  // Rotate just before the old relay socket expires, using the spare ticket.
  // This is the only intentional expiry-driven reconnection.
  if(client.connected() && freshTicket && socketUntil && (int32_t)(socketUntil-millis())<=10000){
    client.stop();demand=false;nextTry=0;
    if(xSemaphoreTake(lock,pdMS_TO_TICKS(20))==pdTRUE){length=0;xSemaphoreGive(lock);}
    strlcpy(state,"reconnecting",sizeof(state));
  }
  if(!client.connected()){
    if(!freshTicket || !lease.valid(millis(),3000)){
      lease.invalidate();freshTicket=false;demand=false;
      strlcpy(state,"reconnecting",sizeof(state));
      return;
    }
    if((int32_t)(millis()-nextTry)<0)return;
    nextTry=millis()+5000;
    if(!connect()){
      // Unauthorized/used tickets cannot be retried. Obtain a new one.
      if(handshakeCode>=400){lease.invalidate();freshTicket=false;}
      client.stop();
      strlcpy(state,"reconnecting",sizeof(state));
      return;
    }
  }
  for(int i=0;i<4&&client.available();i++){
    uint8_t h[2];if(!readExact(h,2)){close();return;}
    size_t n=h[1]&127;if(n==127){close();return;}
    if(n==126){uint8_t x[2];if(!readExact(x,2)){close();return;}n=(x[0]<<8)|x[1];}
    if(!(h[0]&128)||(h[1]&128)||n>2048){close();return;}
    static uint8_t bytes[2049];if(!readExact(bytes,n)){close();return;}
    bytes[n]=0;uint8_t op=h[0]&15;
    if(op==8){close();return;}
    if(op==9){send(10,bytes,n);continue;}
    if(op==10)continue;
    if(op!=1){close();return;}
    static StaticJsonDocument<3072>d;d.clear();
    if(deserializeJson(d,bytes,n))continue;
    if(d["type"]=="command"){
      String reply;
      if(RemoteControl::receive(d.as<JsonObject>(),reply))send(1,(const uint8_t*)reply.c_str(),reply.length());
      continue;
    }
    if(d["type"]=="ready" && d["server_ms"].is<uint64_t>()){
      serverClock=d["server_ms"];clockAt=millis();
    }
    if(d["type"]=="frame_ack"){acked++;lastAck=millis();}
    if(d["type"]=="drop"){strlcpy(lastError,d["reason"]|"Frame rejected",sizeof(lastError));}
    if(d["type"]=="demand"){
      demand=d["run"]|false;
      quality=min(2,d["quality"]|0);
      if(demand&&!frame&&psramFound()){
        xSemaphoreTake(lock,portMAX_DELAY);
        frame=(uint8_t*)heap_caps_malloc(131072+24,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        xSemaphoreGive(lock);
      }
      if(!frame){
        demand=false;
        strlcpy(lastError,"PSRAM frame buffer unavailable",sizeof(lastError));
        blocked=3;
      }
    }
  }
  if(millis()-lastStatus>3000&&(Engine::paused()||blocked.load())){
    lastStatus=millis();
    const char*why=Engine::paused()?"camera_paused":blocked==1?"recording_priority":blocked==2?"frame_too_large":"memory_limited";
    String info=String("{\"type\":\"status\",\"state\":\"")+why+"\"}";
    send(1,(const uint8_t*)info.c_str(),info.length());
  }
  String reply;
  if(RemoteControl::drain(reply))send(1,(const uint8_t*)reply.c_str(),reply.length());
  if(!demand&&frame&&xSemaphoreTake(lock,0)==pdTRUE){
    free(frame);frame=nullptr;length=0;xSemaphoreGive(lock);
  }
  if(xSemaphoreTake(lock,0)==pdTRUE){
    if(length){
      uint32_t id=++seq;
      memcpy(frame,"RVJ1",4);
      for(int i=0;i<4;i++){
        frame[4+i]=id>>(24-8*i);
        frame[12+i]=(length-24)>>(24-8*i);
      }
      frame[8]=width>>8;frame[9]=width;
      frame[10]=height>>8;frame[11]=height;
      uint64_t epochMs=serverClock+int32_t(uint32_t(captured)-clockAt);
      for(int i=0;i<8;i++)frame[16+i]=epochMs>>(56-i*8);
      uint32_t at=micros();size_t bytes=length;
      bool ok=send(2,frame,length);
      lastSendUs=micros()-at;
      maxSendUs=max(maxSendUs,lastSendUs);
      length=0;
      if(ok){sent++;sentBytes+=bytes;}
      else{client.stop();demand=false;}
    }
    xSemaphoreGive(lock);
  }
  if(millis()-lastPing>20000){
    lastPing=millis();
    // Cloudflare Durable Object auto-replies to the text heartbeat 'ping'.
    const uint8_t ping[]="ping";
    if(!send(1,ping,4))close();
  }
}

void status(JsonObject o){
  o["state"]=state;
  o["handshake_http"]=handshakeCode;
  o["connect_attempts"]=connectAttempts;
  o["acknowledged"]=acked;
  o["last_ack_age_ms"]=lastAck?millis()-lastAck:UINT32_MAX;
  o["error"]=lastError;
  o["relay_host"]=lastHost;
  o["tls_result"]=client.result;
  o["tls_verify"]=client.verification;
  o["ticket_remaining_ms"]=max(0,int32_t(lease.until-millis()));
  o["socket_remaining_ms"]=max(0,int32_t(socketUntil-millis()));
  o["viewing"]=demand.load();
  o["sent"]=sent;
  o["dropped"]=dropped;
  o["quality"]=quality.load();
  o["bytes"]=sentBytes;
  o["last_send_ms"]=lastSendUs/1000.0;
  o["max_send_ms"]=maxSendUs/1000.0;
}
}
