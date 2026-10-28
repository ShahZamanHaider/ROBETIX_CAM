#pragma once
#include "Storage.h"
#include "AviWriter.h"
class Recording {
 struct Adapter {
  File file;uint8_t buffer[4096];size_t pending=0;bool bad=false;
  bool drain(){if(bad)return false;if(pending&&file.write(buffer,pending)!=pending)bad=true;pending=0;return !bad;}
  size_t write(const uint8_t*p,size_t n){size_t done=0;while(done<n&&!bad){size_t count=min(n-done,sizeof(buffer)-pending);memcpy(buffer+pending,p+done,count);pending+=count;done+=count;if(pending==sizeof(buffer)&&!drain())return 0;}return bad?0:done;}
  size_t read(uint8_t*p,size_t n){return flush()?file.read(p,n):0;}
  bool seek(uint32_t p){return flush()&&file.seek(p);}
  bool flush(){if(!drain())return false;file.flush();return !file.getWriteError();}
  void yield(){delay(1);}
 } io;
 Avi::Writer<Adapter> writer{io};String temp;uint32_t firstMs=0,lastMs=0,lastFlush=0;bool failed=false;
public:
 String name;bool active=false;uint32_t frames=0,writeUs=0;uint8_t fps=5;
 bool start(camera_fb_t*frame,uint32_t epoch,uint8_t rate){
  name=Storage::newName("avi",epoch);if(name.isEmpty())return false;temp=Storage::path(name,"videos")+".part";if(SD_MMC.exists(temp)||SD_MMC.exists(Storage::path(name,"videos")))return false;
  io.file=SD_MMC.open(temp,"w+");if(!io.file)return false;io.pending=0;io.bad=false;fps=rate;frames=0;failed=false;firstMs=lastMs=lastFlush=millis();active=writer.begin(frame->width,frame->height);if(!active)io.file.close();return active;
 }
 bool append(camera_fb_t*f){uint32_t start=micros();bool ok=active&&writer.append(f->buf,f->len);frames=writer.frames;lastMs=millis();if(millis()-lastFlush>=1000){ok=io.flush()&&ok;lastFlush=millis();}writeUs=micros()-start;failed|=!ok;return ok;}
 bool finish(bool timelapse=false){bool ok=active&&!failed&&writer.finish(timelapse?frames*200:lastMs-firstMs+1000/fps);io.file.close();active=false;if(ok)ok=SD_MMC.rename(temp,Storage::path(name,"videos"));Storage::refresh();if(!ok)Storage::fail("Video save failed. An unfinished .part file may remain on the card.");return ok;}
};
