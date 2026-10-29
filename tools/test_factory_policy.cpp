#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../src/FactoryPolicy.h"
#include <assert.h>
#include <stdio.h>
int main(){bool occupied[100]={};assert(FactoryPolicy::number(0,occupied)==1);occupied[1]=true;assert(FactoryPolicy::number(0,occupied)==2);occupied[99]=true;assert(FactoryPolicy::number(98,occupied)==2);for(int i=1;i<100;i++)occupied[i]=true;assert(!FactoryPolicy::number(33,occupied));assert(FactoryPolicy::randomAccepted(4199999999U));assert(!FactoryPolicy::randomAccepted(4200000000U));assert(!FactoryPolicy::randomAccepted(UINT32_MAX));char digits[9];snprintf(digits,sizeof(digits),"%08lu",(unsigned long)FactoryPolicy::digits(0));assert(!strcmp(digits,"00000000"));assert(FactoryPolicy::numeric8(digits));snprintf(digits,sizeof(digits),"%08lu",(unsigned long)FactoryPolicy::digits(4199999999U));assert(!strcmp(digits,"99999999"));assert(FactoryPolicy::numeric8(digits));assert(!FactoryPolicy::numeric8("1234567"));assert(!FactoryPolicy::numeric8("123456789"));assert(!FactoryPolicy::numeric8("1234a678"));puts("PASS: bounded SSID collision handling, unbiased eight-digit password domain and private-label format.");}
