#pragma once
#include <stdint.h>
struct PairingPolicy {
 uint32_t began=0,next=0,backoff=2000;bool active=false;
 void start(uint32_t now){began=next=now;backoff=2000;active=true;}
 bool expired(uint32_t now)const{return active&&uint32_t(now-began)>=300000;}
 bool ready(uint32_t now)const{return active&&!expired(now)&&int32_t(now-next)>=0;}
 void failed(uint32_t now){next=now+backoff;backoff=backoff<15000?backoff*2:30000;}
 void done(){active=false;}
 static bool permanent(int code){return code>=400&&code<500&&code!=408&&code!=425&&code!=429;}
};
