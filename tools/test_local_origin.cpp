#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../src/LocalOriginPolicy.h"
#include "../src/LocalSocketPolicy.h"
#include <assert.h>
#include <stdio.h>
int main(){using LocalOriginPolicy::permit;const char*url="http://192.168.4.1";
 uint8_t mapped[16]={0,0,0,0,0,0,0,0,0,0,0xff,0xff,192,168,4,2};uint32_t address=0,expected=0;memcpy(&expected,mapped+12,4);assert(LocalSocketPolicy::mappedIpv4(mapped,address));assert(address==expected);mapped[0]=0x20;assert(!LocalSocketPolicy::mappedIpv4(mapped,address));mapped[0]=0;mapped[10]=0;assert(!LocalSocketPolicy::mappedIpv4(mapped,address));mapped[10]=0xff;mapped[11]=0;assert(!LocalSocketPolicy::mappedIpv4(mapped,address));
 assert(permit(url,url,"",""));assert(permit(url,"","http://192.168.4.1/",""));assert(permit(url,"","http://192.168.4.1/settings",""));assert(permit(url,"","","same-origin"));
 assert(!permit(url,"https://attacker.test","http://192.168.4.1/","same-origin"));assert(!permit(url,"null","http://192.168.4.1/","same-origin"));assert(!permit(url,"","http://192.168.4.1.attacker.test/","same-origin"));assert(!permit(url,"","http://192.168.4.10/",""));assert(!permit(url,"","http://192.168.4.1@attacker.test/",""));assert(!permit(url,"","","same-site"));assert(!permit(url,"","",""));puts("PASS: explicit same-origin proof, missing-Origin browser fallback, mismatched/null Origin and lookalike host rejection.");}
