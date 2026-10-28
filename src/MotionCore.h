#pragma once
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
// Bounded 64x48 luminance detector. No model, full-frame RGB buffer or heap allocation.
class MotionCore {
 uint8_t previous[3072]={},noise[3072]={},changed[3072]={};int16_t background[3072]={};
 uint32_t since=0,lastMove=0;unsigned warmup=0;bool armed=true,confirmed=false;
public:
 enum State {Monitoring,Possible,Confirmed,Cooldown};State state=Monitoring;
 unsigned changedPercent=0,localScore=0;uint8_t regions[48]={};
 void reset(){warmup=0;since=lastMove=0;armed=true;confirmed=false;state=Monitoring;changedPercent=localScore=0;memset(regions,0,sizeof(regions));}
 void rebase(){warmup=0;since=0;state=armed?Monitoring:Cooldown;}
 bool active(uint32_t now,uint32_t tail)const{return confirmed&&uint32_t(now-lastMove)<tail;}
 bool sample(const uint8_t*grid,uint32_t now,unsigned sensitivity,unsigned minSeconds,unsigned cooldown,uint16_t x,uint16_t y,uint16_t w,uint16_t h,unsigned customThreshold=12,unsigned customCells=4,unsigned customNeighbors=1){
  int mean=0,oldMean=0;for(unsigned i=0;i<3072;i++){mean+=grid[i];oldMean+=previous[i];}mean/=3072;oldMean/=3072;int shift=mean-oldMean;
  if(warmup++<4){for(unsigned i=0;i<3072;i++){previous[i]=grid[i];background[i]=(int(grid[i])-mean)*16;noise[i]=2;}return false;}
  unsigned threshold=sensitivity==0?24:sensitivity==1?16:sensitivity==2?10:customThreshold;
  unsigned neighborsRequired=sensitivity<2?2:sensitivity==2?1:customNeighbors;
  unsigned localRequired=sensitivity==0?14:sensitivity==1?7:sensitivity==2?4:customCells;
  memset(regions,0,sizeof(regions));uint16_t blockTotal[48]={};
  for(unsigned i=0;i<3072;i++){
   int value=int(grid[i])-mean,d=abs(int(grid[i])-previous[i]-shift),b=abs(value-background[i]/16);
   unsigned adaptive=threshold+noise[i]/2;changed[i]=(d>int(adaptive)||b>int(adaptive+3))&&abs(shift)<25;
   if(d<int(threshold))noise[i]=(noise[i]*7+d)/8;
   // Foreground is absorbed slowly; steady scene/exposure changes converge quickly.
   background[i]+=(value*16-background[i])/(d>int(adaptive)?64:4);previous[i]=grid[i];
  }
  unsigned total=0,moved=0;
  for(unsigned row=1;row<47;row++)for(unsigned col=1;col<63;col++){
   unsigned px=col*1000/64,py=row*1000/48;if(px<x||py<y||px>=x+w||py>=y+h)continue;
   unsigned i=row*64+col,block=(row/8)*8+col/8;total++;blockTotal[block]++;unsigned neighbors=0;
   for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)if(dx||dy)neighbors+=changed[int(i)+dy*64+dx];
   if(changed[i]&&neighbors>=neighborsRequired){moved++;regions[block]++;}
  }
  changedPercent=total?100*moved/total:0;localScore=0;
  for(unsigned i=0;i<48;i++)if(blockTotal[i]>=localRequired&&regions[i]>localScore)localScore=regions[i];
  bool movement=abs(shift)<25&&(localScore>=localRequired||changedPercent>=(sensitivity==0?12:sensitivity==1?6:2));
  if(movement){lastMove=now;if(!since)since=now;state=armed?Possible:Cooldown;if(armed&&uint32_t(now-since)>=minSeconds*1000UL){armed=false;confirmed=true;state=Confirmed;return true;}}
  else{since=0;if(uint32_t(now-lastMove)>=cooldown*1000UL){armed=true;confirmed=false;state=Monitoring;}else state=armed?Monitoring:Cooldown;}
  return false;
 }
};
