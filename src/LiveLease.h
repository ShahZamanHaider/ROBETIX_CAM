#pragma once
#include <stdint.h>
struct LiveLease {
 uint32_t until=0;bool required=true;
 void invalidate(){required=true;until=0;}
 void accept(uint64_t serverMs,uint32_t expires,uint32_t now){int64_t remaining=int64_t(uint64_t(expires)*1000)-int64_t(serverMs);until=now+uint32_t(remaining>90000?90000:remaining<0?0:remaining);required=false;}
 bool valid(uint32_t now,uint32_t margin=2000)const{return !required&&int32_t(until-now)>int32_t(margin);}
};
