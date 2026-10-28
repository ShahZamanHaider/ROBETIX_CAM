#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../src/PairingPolicy.h"
#include "../src/RecoveryWindow.h"
#include "../src/LocalAccessWindow.h"
#include <assert.h>
#include <stdio.h>
int main(){LocalAccessWindow local;assert(!local.permit(0));local.start(100);assert(local.permit(101));assert(!local.permit(60000100));local.start(0);for(int i=0;i<5;i++)assert(local.permit(i));for(int i=0;i<1000;i++)assert(!local.permit(i));local.start(0);local.finish();assert(!local.permit(1));PairingPolicy p;p.start(100);assert(p.ready(100));p.failed(100);assert(!p.ready(2099));assert(p.ready(2100));for(int i=0;i<10;i++)p.failed(2000*i);assert(p.backoff==30000);assert(p.expired(300100));assert(!p.ready(300100));assert(PairingPolicy::permanent(409));assert(!PairingPolicy::permanent(429));assert(!PairingPolicy::permanent(503));p.done();assert(!p.ready(999999));p.start(0xfffffff0U);assert(!p.expired(100));RecoveryWindow r;assert(!r.permit(100));r.start(100);for(int i=0;i<5;i++)assert(r.permit(101+i));assert(!r.permit(110));r.start(100);assert(!r.permit(300000100));r.start(500);r.finish();assert(!r.permit(600));puts("PASS: bounded pairing retry, expiry, HTTP error classification; physical recovery expiry, five attempts, one-use completion and rearming.");}
