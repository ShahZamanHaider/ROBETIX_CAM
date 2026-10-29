#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace FactoryPolicy {
// A short SSID is a convenience name, never the security/device identity.
// Probe the full 01..99 space against a nearby scan; zero means it is full.
inline unsigned number(uint64_t chip,const bool occupied[100]){
 unsigned first=unsigned(chip%99)+1;
 for(unsigned offset=0;offset<99;offset++){unsigned n=(first-1+offset)%99+1;if(!occupied[n])return n;}
 return 0;
}
inline bool password(const char*ssid,char*out,size_t capacity){
 size_t length=strlen(ssid);if(length<5||length>32||capacity<length+4)return false;
 for(size_t i=0;i<length;i++){unsigned char c=ssid[i];out[i]=(c>='A'&&c<='Z')?c+('a'-'A'):c;}
 memcpy(out+length,"786",4);return true;
}
}
