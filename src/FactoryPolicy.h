#pragma once
#include <stdint.h>
namespace FactoryPolicy {
// A short SSID is a convenience name, never the security/device identity.
// Probe the full 01..99 space against a nearby scan; zero means it is full.
inline unsigned number(uint64_t chip,const bool occupied[100]){
 unsigned first=unsigned(chip%99)+1;
 for(unsigned offset=0;offset<99;offset++){unsigned n=(first-1+offset)%99+1;if(!occupied[n])return n;}
 return 0;
}
inline bool randomAccepted(uint32_t value){return value<4200000000UL;}
inline uint32_t digits(uint32_t value){return value%100000000UL;}
}
