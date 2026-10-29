#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../src/FactoryPolicy.h"
#include <assert.h>
#include <stdio.h>
int main(){bool occupied[100]={};assert(FactoryPolicy::number(0,occupied)==1);occupied[1]=true;assert(FactoryPolicy::number(0,occupied)==2);occupied[99]=true;assert(FactoryPolicy::number(98,occupied)==2);for(int i=1;i<100;i++)occupied[i]=true;assert(!FactoryPolicy::number(33,occupied));assert(FactoryPolicy::randomAccepted(4199999999U));assert(!FactoryPolicy::randomAccepted(4200000000U));assert(!FactoryPolicy::randomAccepted(UINT32_MAX));assert(FactoryPolicy::digits(0)==0);assert(FactoryPolicy::digits(4199999999U)==99999999);puts("PASS: bounded 01..99 collision probing, saturation and unbiased eight-digit password domain.");}
