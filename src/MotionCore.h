#pragma once
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
// Portable grid comparison, shared with synthetic host checks. No AI model.
class MotionCore {
 uint8_t previous[768]={};uint32_t since=0,lastMove=0;unsigned warmup=0;bool armed=true,confirmed=false;
public:
 unsigned changedPercent=0;
 void reset(){warmup=0;since=lastMove=0;armed=true;confirmed=false;}
 void rebase(){warmup=0;since=0;}
 bool active(uint32_t now,uint32_t tail)const{return confirmed&&uint32_t(now-lastMove)<tail;}
 bool sample(const uint8_t*grid,uint32_t now,unsigned sensitivity,unsigned minSeconds,unsigned cooldown,uint16_t x,uint16_t y,uint16_t w,uint16_t h){
  if(warmup++<3){memcpy(previous,grid,768);return false;}
  int shift=0;for(unsigned i=0;i<768;i++)shift+=int(grid[i])-previous[i];shift/=768;
  uint8_t changed[768]={};unsigned threshold=sensitivity==0?24:sensitivity==1?18:12;
  for(unsigned i=0;i<768;i++)changed[i]=abs(int(grid[i])-previous[i]-shift)>int(threshold);
  memcpy(previous,grid,768);
  unsigned total=0,moved=0;
  for(unsigned row=1;row<23;row++)for(unsigned col=1;col<31;col++){
   unsigned px=col*1000/32,py=row*1000/24;if(px<x||py<y||px>=x+w||py>=y+h)continue;total++;
   unsigned i=row*32+col,neighbors=0;for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)if(dx||dy)neighbors+=changed[int(i)+dy*32+dx];if(changed[i]&&neighbors>=2)moved++;
  }
  changedPercent=total?100*moved/total:0;
  bool movement=abs(shift)<25&&changedPercent>=(sensitivity==0?12:sensitivity==1?8:4);
  if(movement){lastMove=now;if(!since)since=now;if(armed&&uint32_t(now-since)>=minSeconds*1000UL){armed=false;confirmed=true;return true;}}
  else{since=0;if(uint32_t(now-lastMove)>=cooldown*1000UL){armed=true;confirmed=false;}}
  return false;
 }
};
