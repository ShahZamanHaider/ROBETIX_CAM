#pragma once
// Independent, bounded, video-only RIFF/AVI writer. Shared by firmware and host tests.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace Avi {
constexpr uint32_t HEADER_SIZE=224, MAX_FRAMES=3000, MAX_BYTES=256UL*1024*1024;
inline void u16(uint8_t*p,uint16_t v){p[0]=v;p[1]=v>>8;}
inline void u32(uint8_t*p,uint32_t v){for(int i=0;i<4;i++)p[i]=v>>(i*8);}
inline uint32_t get32(const uint8_t*p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
inline void header(uint8_t*h,uint32_t width,uint32_t height,uint32_t frames,uint32_t elapsedMs,uint32_t movieBytes,uint32_t fileBytes,uint32_t maxFrame){
 memset(h,0,HEADER_SIZE);memcpy(h,"RIFF",4);u32(h+4,fileBytes-8);memcpy(h+8,"AVI LIST",8);u32(h+16,192);memcpy(h+20,"hdrlavih",8);u32(h+28,56);
 uint32_t rate=frames&&elapsedMs?uint64_t(frames)*1000000/elapsedMs:5000;
 u32(h+32,frames?uint64_t(elapsedMs)*1000/frames:200000);u32(h+36,uint64_t(maxFrame)*rate/1000);u32(h+44,frames?0x10:0);u32(h+48,frames);u32(h+56,1);u32(h+60,maxFrame);u32(h+64,width);u32(h+68,height);
 memcpy(h+88,"LIST",4);u32(h+92,116);memcpy(h+96,"strlstrh",8);u32(h+104,56);memcpy(h+108,"vidsMJPG",8);u32(h+128,1000);u32(h+132,rate);u32(h+140,frames);u32(h+144,maxFrame);u32(h+148,0xffffffff);u16(h+160,width);u16(h+162,height);
 memcpy(h+164,"strf",4);u32(h+168,40);u32(h+172,40);u32(h+176,width);u32(h+180,height);u16(h+184,1);u16(h+186,24);memcpy(h+188,"MJPG",4);u32(h+192,width*height*3);
 memcpy(h+212,"LIST",4);u32(h+216,4+movieBytes);memcpy(h+220,"movi",4);
}
template<class IO> class Writer {
 IO& io;uint32_t width=0,height=0,movieBytes=0,maxFrame=0;bool failed=false;
 bool put(const void*p,size_t n){if(failed)return false;if(io.write((const uint8_t*)p,n)!=n)failed=true;return !failed;}
public:
 uint32_t frames=0;
 explicit Writer(IO&source):io(source){}
 bool begin(uint32_t w,uint32_t h){width=w;height=h;movieBytes=maxFrame=frames=0;failed=false;uint8_t b[HEADER_SIZE];header(b,w,h,0,0,0,HEADER_SIZE,0);return io.seek(0)&&put(b,sizeof(b));}
 bool append(const uint8_t*jpeg,size_t length){
  if(failed||frames>=MAX_FRAMES||length<4||length>512*1024||movieBytes+length+8+MAX_FRAMES*16>MAX_BYTES)return false;
  if(jpeg[0]!=0xff||jpeg[1]!=0xd8||jpeg[length-2]!=0xff||jpeg[length-1]!=0xd9)return false;
  uint8_t b[8];memcpy(b,"00dc",4);u32(b+4,length);uint8_t pad=0;
  if(!put(b,8)||!put(jpeg,length)||(length&1&&!put(&pad,1)))return false;
  movieBytes+=8+length+(length&1);if(length>maxFrame)maxFrame=length;frames++;return true;
 }
 bool finish(uint32_t elapsedMs){
  if(failed||!frames||!io.flush())return false;
  uint32_t end=HEADER_SIZE+movieBytes;uint8_t marker[8];memcpy(marker,"idx1",4);u32(marker+4,frames*16);
  if(!io.seek(end)||!put(marker,8))return false;
  uint32_t pos=HEADER_SIZE,out=end+8;
  for(uint32_t base=0;base<frames;base+=32){
   uint8_t entries[512];uint32_t count=frames-base<32?frames-base:32;
   for(uint32_t i=0;i<count;i++){
    uint8_t chunk[8];if(!io.seek(pos)||io.read(chunk,8)!=8||memcmp(chunk,"00dc",4))return false;
    uint32_t len=get32(chunk+4);if(len<4||len>512*1024||pos+8+len+(len&1)>end)return false;
    uint8_t*e=entries+i*16;memcpy(e,"00dc",4);u32(e+4,0x10);u32(e+8,pos-220);u32(e+12,len);pos+=8+len+(len&1);
   }
   if(!io.seek(out)||!put(entries,count*16))return false;out+=count*16;io.yield();
  }
  if(pos!=end)return false;
  uint8_t b[HEADER_SIZE];header(b,width,height,frames,elapsedMs?elapsedMs:1,movieBytes,out,maxFrame);
  return io.seek(0)&&put(b,sizeof(b))&&io.flush();
 }
};
}
