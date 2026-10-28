#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../src/MotionCore.h"
#include "../src/Schedule.h"
#include <assert.h>
#include <stdio.h>
int main(){
 assert(!recordingWindow(0,0,1320,360,127));
 // 2024-01-01 Monday 23:00 UTC, Tuesday 05:00 belongs to Monday overnight window.
 assert(recordingWindow(1704150000,0,1320,360,2));
 assert(recordingWindow(1704171600,0,1320,360,2));
 assert(!recordingWindow(1704175200,0,1320,360,2));
 assert(!recordingWindow(1704150000,0,1320,360,1));
 MotionCore m;uint8_t grid[768];memset(grid,80,sizeof(grid));
 for(unsigned t=1000;t<=6000;t+=1000)assert(!m.sample(grid,t,1,2,30,0,0,1000,1000));
 memset(grid,110,sizeof(grid));assert(!m.sample(grid,7000,1,2,30,0,0,1000,1000));
 bool detected=false;for(unsigned t=8000;t<=11000;t+=1000){for(int y=5;y<15;y++)for(int x=5;x<15;x++)grid[y*32+x]=(t/1000)%2?180:60;detected|=m.sample(grid,t,1,2,30,0,0,1000,1000);}assert(detected);assert(m.active(11000,15000));
 for(unsigned t=12000;t<=44000;t+=1000)assert(!m.sample(grid,t,1,2,30,0,0,1000,1000));assert(!m.active(44000,15000));
 m.reset();memset(grid,80,sizeof(grid));for(unsigned t=1000;t<=3000;t+=1000)m.sample(grid,t,1,1,5,700,0,300,1000);
 for(unsigned t=4000;t<=8000;t+=1000){for(int y=5;y<15;y++)for(int x=5;x<15;x++)grid[y*32+x]=(t/1000)%2?180:60;assert(!m.sample(grid,t,1,1,5,700,0,300,1000));}
 puts("PASS: schedule invalid clock, overnight day ownership and end boundary; motion noise, global light rejection, confirmation, cooldown and excluded zone.");
}
