#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../src/NetworkPolicy.h"
#include <assert.h>
#include <stdio.h>
int main(){NetworkPolicy p;p.reset(100);assert(p.ready(100));p.started(100);for(uint32_t t=101;t<45100;t+=100){assert(!p.ready(t));assert(!p.timedOut(t));}assert(p.timedOut(45100));p.failed(45100);assert(!p.ready(50099));assert(p.ready(50100));p.started(50100);p.failed(95100);assert(!p.ready(105099));assert(p.ready(105100));p.connected(110000);assert(!p.ready(114999));assert(p.ready(115000));assert(p.backoff==5000);for(int i=0;i<12;i++)p.failed(i*100000);assert(p.backoff==90000);p.reset(0xfffffff0U);p.started(0xfffffff0U);assert(!p.timedOut(0xfffffff8U));assert(p.timedOut(uint32_t(0xfffffff0U+45000U)));puts("PASS: active association is never restarted; 45-second deadline, bounded retry backoff, reconnect reset and millis rollover.");}
