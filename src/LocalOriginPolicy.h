#pragma once
#include <string.h>
namespace LocalOriginPolicy {
// expected is constructed only after the receiving camera's Host is validated.
// A supplied mismatching Origin must never fall back to another header.
inline bool permit(const char*expected,const char*origin,const char*referer,const char*site){
 if(origin&&*origin)return !strcmp(expected,origin);
 size_t n=strlen(expected);
 if(referer&&*referer)return !strncmp(expected,referer,n)&&referer[n]=='/';
 return site&&!strcmp(site,"same-origin");
}
}
