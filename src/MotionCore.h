#pragma once
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
// Luminance/foreground detector; all storage is fixed and owned by the capture task.
class MotionCore {
 uint8_t previous[3072]={},noise[3072]={},changed[3072]={},foreground[3072]={},known[3072]={};
 int16_t background[3072]={};uint32_t sampleAt=0,since=0,lastMove=0,eventAt=0,quietAt=0;unsigned warmup=0,confirmSamples=0;bool hadEvent=false,inEvent=false,quiet=false,preserveReference=false;
 void learn(const uint8_t*g,int mean){for(unsigned i=0;i<3072;i++){previous[i]=g[i];background[i]=(int(g[i])-mean)*16;noise[i]=2;known[i]=0;}}
public:
 enum State {Monitoring,Possible,Confirmed,EventActive,MotionEnded,Ready,Cooldown};State state=Monitoring;
 unsigned changedPercent=0,localScore=0;uint8_t regions[192]={};bool ignoredScene=false,detected=false;
 void reset(){warmup=0;sampleAt=since=lastMove=eventAt=quietAt=0;confirmSamples=0;hadEvent=inEvent=quiet=preserveReference=false;state=Monitoring;changedPercent=localScore=0;memset(regions,0,sizeof(regions));}
 void rebase(){preserveReference=warmup>=4;warmup=0;since=0;confirmSamples=0;quiet=false;state=inEvent?EventActive:Monitoring;}
 bool active(uint32_t now,uint32_t tail)const{return hadEvent&&uint32_t(now-lastMove)<tail;}
 bool sample(const uint8_t*g,uint32_t now,unsigned sensitivity,unsigned minSeconds,unsigned cooldown,uint16_t x,uint16_t y,uint16_t w,uint16_t h,unsigned customThreshold=12,unsigned customCells=4,unsigned customNeighbors=1,bool smaller=false){
  ignoredScene=detected=false;int mean=0,oldMean=0;for(unsigned i=0;i<3072;i++){mean+=g[i];oldMean+=previous[i];}mean/=3072;oldMean/=3072;int shift=mean-oldMean;
  if(warmup<4){warmup++;if(preserveReference){memcpy(previous,g,3072);}else learn(g,mean);sampleAt=now;if(warmup==4)preserveReference=false;return false;}
  if(uint32_t(now-sampleAt)>3000){learn(g,mean);sampleAt=now;since=0;confirmSamples=0;return false;}sampleAt=now;
  unsigned threshold=sensitivity==0?24:sensitivity==1?16:sensitivity==2?10:customThreshold;
  unsigned neighborsRequired=sensitivity<2?2:sensitivity==2?1:customNeighbors;
  unsigned localRequired=sensitivity==0?10:sensitivity==1?6:sensitivity==2?4:customCells;if(smaller){localRequired=localRequired>4?localRequired/2:3;neighborsRequired=1;}
  // Reject a strong whole-frame shift matching a one-cell camera translation.
  unsigned zero=0,best=0;for(unsigned row=2;row<46;row+=2)for(unsigned col=2;col<62;col+=2){int d=abs(int(g[row*64+col])-previous[row*64+col]-shift);zero+=d>25?25:d;}best=zero;
  if(zero>2640)for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)if(dx||dy){unsigned cost=0;for(unsigned row=2;row<46;row+=2)for(unsigned col=2;col<62;col+=2){int d=abs(int(g[row*64+col])-previous[(int(row)+dy)*64+int(col)+dx]-shift);cost+=d>25?25:d;}if(cost<best)best=cost;}
  if(abs(shift)>=25||(zero>2640&&best*100<zero*60)){learn(g,mean);since=0;confirmSamples=0;ignoredScene=true;memset(regions,0,sizeof(regions));localScore=changedPercent=0;return false;}
  memset(regions,0,sizeof(regions));uint8_t blockActivity[192]={},eligible[192]={},totalBlock[192]={};unsigned total=0,moved=0;
  for(unsigned i=0;i<3072;i++){int value=int(g[i])-mean,d=abs(int(g[i])-previous[i]-shift),b=abs(value-background[i]/16);unsigned adaptive=threshold+noise[i]/2;changed[i]=d>int(adaptive);foreground[i]=b>int(adaptive+2);if(b<int(adaptive/2))known[i]=0;if(d<int(threshold))noise[i]=(noise[i]*7+d)/8;
   // Freeze a new foreground until confirmation; very slowly absorb settled known objects.
   if(!foreground[i])background[i]+=(value*16-background[i])/12;else if(known[i])background[i]+=(value*16-background[i])/256;previous[i]=g[i];
  }
  for(unsigned row=1;row<47;row++)for(unsigned col=1;col<63;col++){unsigned px=col*1000/64,py=row*1000/48;if(px<x||py<y||px>=x+w||py>=y+h)continue;unsigned i=row*64+col,b=(row/4)*16+col/4,fn=0,cn=0;total++;totalBlock[b]++;for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)if(dx||dy){unsigned n=int(i)+dy*64+dx;fn+=foreground[n];cn+=changed[n];}bool fg=foreground[i]&&fn>=neighborsRequired,activity=changed[i]&&cn>=neighborsRequired;if(activity||(fg&&!known[i]))regions[b]++;if(fg){moved++;if(!known[i])eligible[b]++;}if(activity)blockActivity[b]++;}
  bool candidate=false,activity=false;localScore=0;for(unsigned b=0;b<192;b++){if(regions[b]>localScore)localScore=regions[b];if(totalBlock[b]>=localRequired){candidate|=eligible[b]>=localRequired;activity|=blockActivity[b]>=localRequired;}}changedPercent=total?100*moved/total:0;
  // Also count a small cluster straddling adjacent 4x4 region boundaries.
  if(smaller||localRequired>16)for(unsigned r=0;r<11;r++)for(unsigned c=0;c<15;c++){unsigned b=r*16+c;candidate|=unsigned(eligible[b])+eligible[b+1]+eligible[b+16]+eligible[b+17]>=localRequired;activity|=unsigned(blockActivity[b])+blockActivity[b+1]+blockActivity[b+16]+blockActivity[b+17]>=localRequired;}
  detected=candidate||activity;
  if(inEvent){state=EventActive;if(activity){lastMove=now;quiet=false;}else{if(!quiet){quiet=true;quietAt=now;}if(uint32_t(now-quietAt)>=750){inEvent=false;state=MotionEnded;since=0;confirmSamples=0;quiet=false;}}for(unsigned i=0;i<3072;i++)if(foreground[i])known[i]=1;return false;}
  if(state==MotionEnded){state=Ready;return false;}if(state==Ready)state=Monitoring;
  if(!candidate){since=0;confirmSamples=0;state=Monitoring;return false;}
  if(!confirmSamples)since=now;confirmSamples++;state=Possible;
  if(confirmSamples<2||uint32_t(now-since)<minSeconds*1000UL)return false;
  if(hadEvent&&uint32_t(now-eventAt)<cooldown*1000UL){state=Cooldown;return false;}
  inEvent=hadEvent=true;quiet=false;lastMove=eventAt=now;since=0;confirmSamples=0;state=Confirmed;for(unsigned i=0;i<3072;i++)if(foreground[i])known[i]=1;return true;
 }
};
