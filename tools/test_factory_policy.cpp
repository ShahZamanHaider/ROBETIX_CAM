#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../src/FactoryPolicy.h"
#include <assert.h>
#include <stdio.h>
int main(){bool occupied[100]={};assert(FactoryPolicy::number(0,occupied)==1);occupied[1]=true;assert(FactoryPolicy::number(0,occupied)==2);occupied[99]=true;assert(FactoryPolicy::number(98,occupied)==2);for(int i=1;i<100;i++)occupied[i]=true;assert(!FactoryPolicy::number(33,occupied));char value[36];assert(FactoryPolicy::password("ROBETIX-CAM",value,sizeof(value)));assert(!strcmp(value,"robetix-cam786"));assert(FactoryPolicy::password("ROBETIX-VISION-01",value,sizeof(value)));assert(!strcmp(value,"robetix-vision-01786"));assert(FactoryPolicy::password("Gate Camera",value,sizeof(value)));assert(!strcmp(value,"gate camera786"));assert(FactoryPolicy::password("ABCDE",value,sizeof(value)));assert(strlen(value)==8);assert(!FactoryPolicy::password("CAM",value,sizeof(value)));assert(!FactoryPolicy::password("ROBETIX-CAM",value,8));assert(!FactoryPolicy::password("123456789012345678901234567890123",value,sizeof(value)));puts("PASS: bounded device-number collision handling; lowercase SSID + 786, internal spaces, WPA2 minimum and buffer bounds.");}
