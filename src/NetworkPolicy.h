#pragma once
#include <stdint.h>
// Time-based policy, independent of the Wi-Fi driver. Never restart an active attempt.
struct NetworkPolicy {
 bool attempting=false;uint32_t deadline=0,next=0,backoff=5000;
 static bool due(uint32_t now,uint32_t target){return int32_t(now-target)>=0;}
 void reset(uint32_t now){attempting=false;next=now;backoff=5000;}
 bool ready(uint32_t now)const{return !attempting&&due(now,next);}
 void started(uint32_t now){attempting=true;deadline=now+45000;}
 bool timedOut(uint32_t now)const{return attempting&&due(now,deadline);}
 void failed(uint32_t now){attempting=false;next=now+backoff;backoff=backoff<45000?backoff*2:90000;}
 // A Wi-Fi driver disconnect is a completed attempt, not something to leave
 // active until the association deadline expires.
 void disconnected(uint32_t now){failed(now);}
 void connected(uint32_t now){attempting=false;backoff=5000;next=now+5000;}
};
