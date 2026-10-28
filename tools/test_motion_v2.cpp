#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../src/MotionCore.h"
#include <assert.h>
#include <stdio.h>
#include <time.h>
static uint8_t grid[3072];static uint32_t now=1000;static MotionCore detector;
static bool sample(unsigned cooldown=2){bool e=detector.sample(grid,now,1,1,cooldown,0,0,1000,1000,12,4,1);now+=500;return e;}
static void hand(bool present){memset(grid,80,sizeof(grid));if(present)for(unsigned y=14;y<24;y++)for(unsigned x=20;x<30;x++)grid[y*64+x]=170;}
int main(){
 hand(false);for(int i=0;i<6;i++)assert(!sample());unsigned events=0;for(int cycle=0;cycle<3;cycle++){hand(true);for(int i=0;i<5;i++)events+=sample();hand(false);for(int i=0;i<6;i++)assert(!sample());assert(detector.state==MotionCore::Monitoring||detector.state==MotionCore::Ready);}assert(events==3);
 // Continuous movement remains one event, and a minimum interval is measured from event start.
 detector.reset();now=1000;hand(false);for(int i=0;i<6;i++)sample();events=0;for(int i=0;i<20;i++){hand(true);for(unsigned y=14;y<24;y++)for(unsigned x=20;x<30;x++)grid[y*64+x]=(i%2)?160:200;events+=sample();}assert(events==1);assert(detector.active(now,15000));hand(false);for(int i=0;i<8;i++)sample();hand(true);for(int i=0;i<5;i++)events+=sample();assert(events==2);
 detector.reset();now=1000;hand(false);for(int i=0;i<6;i++)sample();hand(true);for(int i=0;i<5;i++)sample(30);hand(false);for(int i=0;i<6;i++)sample(30);hand(true);unsigned extra=0;for(int i=0;i<5;i++)extra+=sample(30);assert(extra==0);for(int i=0;i<60;i++)extra+=sample(30);assert(extra==1);
 // Uniform lighting changes and single-cell flicker do not create events.
 detector.reset();now=1000;hand(false);for(int i=0;i<6;i++)sample();memset(grid,130,sizeof(grid));for(int i=0;i<10;i++)assert(!sample());for(int i=0;i<10;i++){grid[10*64+10]=i%2?220:50;assert(!sample());}
 // Coarse camera translation of a textured scene is rejected.
 detector.reset();now=1000;for(unsigned y=0;y<48;y++)for(unsigned x=0;x<64;x++)grid[y*64+x]=60+((x*17+y*13)%100);for(int i=0;i<6;i++)sample();uint8_t old[3072];memcpy(old,grid,sizeof(old));for(unsigned y=0;y<48;y++)for(unsigned x=1;x<64;x++)grid[y*64+x]=old[y*64+x-1];assert(!sample());assert(detector.ignoredScene);
 // Preserve legacy custom local-cell settings above a single new 4x4 region's capacity.
 detector.reset();hand(false);for(int i=0;i<6;i++){detector.sample(grid,now,3,1,2,0,0,1000,1000,12,32,1);now+=500;}hand(true);events=0;for(int i=0;i<5;i++){events+=detector.sample(grid,now,3,1,2,0,0,1000,1000,12,32,1);now+=500;}assert(events==1);
 clock_t start=clock();for(int i=0;i<1000;i++)sample();long long us=(clock()-start)*1000000LL/CLOCKS_PER_SEC;printf("PASS: three hand entry events, no exit-only duplicates, continuous event, rearm/cooldown, illumination/noise, camera-shift rejection and legacy custom region threshold. Host comparison average: %lld us (not ESP32 timing).\n",(long long)(us/1000));
}
