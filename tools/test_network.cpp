#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../src/NetworkPolicy.h"
#include <assert.h>
#include <stdio.h>
int main(){NetworkPolicy p;p.reset(100);assert(p.ready(100));p.started(100);for(uint32_t t=101;t<45100;t+=100){assert(!p.ready(t));assert(!p.timedOut(t));}assert(p.timedOut(45100));p.failed(45100);assert(!p.ready(50099));assert(p.ready(50100));p.started(50100);p.disconnected(50300);assert(!p.attempting);assert(!p.ready(60299));assert(p.ready(60300));p.started(60300);p.failed(100300);assert(!p.ready(120299));assert(p.ready(120300));p.connected(125000);assert(!p.ready(129999));assert(p.ready(130000));assert(p.backoff==5000);for(int i=0;i<12;i++)p.failed(i*100000);assert(p.backoff==90000);p.reset(0xfffffff0U);p.started(0xfffffff0U);assert(!p.timedOut(0xfffffff8U));assert(p.timedOut(uint32_t(0xfffffff0U+45000U)));puts("PASS: active association is never restarted; disconnects end failed attempts promptly; retry backoff, reconnect reset and millis rollover remain bounded.");}
