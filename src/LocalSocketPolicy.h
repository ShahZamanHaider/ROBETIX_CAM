#pragma once
#include <stdint.h>
#include <string.h>
namespace LocalSocketPolicy {
// ESP-IDF's dual-stack HTTP listener returns ::ffff:a.b.c.d for IPv4
// clients. Preserve network byte order when comparing with lwIP DHCP data.
inline bool mappedIpv4(const uint8_t bytes[16],uint32_t&address){
 for(unsigned i=0;i<10;i++)if(bytes[i])return false;
 if(bytes[10]!=0xff||bytes[11]!=0xff)return false;
 memcpy(&address,bytes+12,4);return true;
}
}
