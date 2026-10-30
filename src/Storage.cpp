#include "Storage.h"
#include "Settings.h"
#include "AviWriter.h"
namespace Storage {
std::atomic<bool> available{false};SemaphoreHandle_t operationMutex;
static SemaphoreHandle_t statsMutex;static uint64_t total=0,used=0;static char lastError[160]={};static uint32_t nextSequence=0,unfinished=0;
static int mountKHz=0,mountAttempts=0,cardType=0;static bool mounted=false;
static constexpr size_t SCAN_LIMIT=4096,PAGE_SIZE=12;
void fail(const char*s){xSemaphoreTake(statsMutex,portMAX_DELAY);strlcpy(lastError,s,sizeof(lastError));xSemaphoreGive(statsMutex);}
void refresh(){if(!available)return;uint64_t t=SD_MMC.totalBytes(),u=SD_MMC.usedBytes();xSemaphoreTake(statsMutex,portMAX_DELAY);total=t;used=u;xSemaphoreGive(statsMutex);}
static uint64_t freeBytes(){xSemaphoreTake(statsMutex,portMAX_DELAY);uint64_t f=total>used?total-used:0;xSemaphoreGive(statsMutex);return f;}
uint32_t sequence(const String&name){return strtoul(name.c_str()+3,nullptr,10);}
bool validName(const String&n,const String&kind){
 if(kind=="legacy"){if(n.length()!=20||!n.endsWith(".jpg"))return false;for(int i=0;i<16;i++)if(!isxdigit((unsigned char)n[i]))return false;return true;}
 if(kind!="videos"&&kind!="photos")return false;
 if(n.length()!=28||!n.startsWith("rv_")||n[13]!='_'||!n.endsWith(kind=="videos"?".avi":".jpg"))return false;
 for(int i=3;i<24;i++)if(i!=13&&(n[i]<'0'||n[i]>'9'))return false;return true;
}
String path(const String&name,const String&kind){if(!validName(name,kind))return "";return (kind=="legacy"?String("/snapshots/"):String("/robetix/")+kind+"/")+name;}
static String baseName(const char*p){String s=p;int slash=s.lastIndexOf('/');return slash>=0?s.substring(slash+1):s;}
void begin(){
 operationMutex=xSemaphoreCreateMutex();statsMutex=xSemaphoreCreateMutex();if(!operationMutex||!statsMutex){available=false;return;}
 pinMode(4,OUTPUT);digitalWrite(4,LOW);
 for(int frequency:{SDMMC_FREQ_DEFAULT,10000,5000}){mountAttempts++;mounted=SD_MMC.begin("/sdcard",true,false,frequency,5);cardType=mounted?SD_MMC.cardType():0;available=mounted&&cardType!=CARD_NONE;mountKHz=frequency;if(available){Serial.printf("SD ready: 1-bit, %d kHz, card type %d, capacity %llu bytes\n",frequency,SD_MMC.cardType(),SD_MMC.cardSize());break;}Serial.printf("SD mount failed at %d kHz; no formatting attempted\n",frequency);SD_MMC.end();delay(100);}
 if(available)for(const char*dir:{"/robetix","/robetix/videos","/robetix/photos"})if(!SD_MMC.exists(dir)&&!SD_MMC.mkdir(dir))available=false;
 if(!available){fail("Memory card unavailable. Power off, check the FAT32 card, then restart.");SD_MMC.end();return;}
 nextSequence=prefs.getUInt("fileSeq",0);
 for(const char*kind:{"videos","photos"}){File dir=SD_MMC.open(String("/robetix/")+kind);size_t count=0;while(File f=dir.openNextFile()){String n=baseName(f.name());if(validName(n,kind))nextSequence=max(nextSequence,sequence(n));if(n.endsWith(".part"))unfinished++;f.close();if(++count>=SCAN_LIMIT)break;if(!(count%32))delay(1);}dir.close();}
 refresh();
}
String newName(const char*ext,uint32_t epoch){
 if(nextSequence==UINT32_MAX){fail("File number limit reached");return "";}
 ++nextSequence;if(!prefs.putUInt("fileSeq",nextSequence)){fail("Could not reserve a file name");return "";}
 char name[40];snprintf(name,sizeof(name),"rv_%010lu_%010lu.%s",(unsigned long)nextSequence,(unsigned long)epoch,ext);return name;
}
bool reserve(uint32_t bytes,uint16_t reserveMB,bool cleanup){
 if(!available)return false;refresh();uint64_t target=uint64_t(reserveMB)*1024*1024+bytes;
 for(int deleted=0;freeBytes()<target&&cleanup&&deleted<8;deleted++){
  File dir=SD_MMC.open("/robetix/videos");if(!dir){fail("Could not read videos folder");return false;}
  uint32_t oldest=UINT32_MAX;String selected;size_t count=0;bool complete=true;
  while(File f=dir.openNextFile()){String n=baseName(f.name());bool eligible=!f.isDirectory()&&validName(n,"videos");f.close();if(eligible&&sequence(n)<oldest){oldest=sequence(n);selected=n;}if(++count>SCAN_LIMIT){complete=false;break;}if(!(count%32))delay(1);}dir.close();
  if(!complete){fail("Too many files for automatic cleanup; remove files manually");return false;}
  if(selected.isEmpty())break;
  if(!SD_MMC.remove(path(selected,"videos"))){fail("Could not remove oldest video; cleanup stopped");return false;}refresh();
 }
 if(freeBytes()<target){fail("Not enough card space. Delete saved files or lower the space reserve.");return false;}return true;
}
bool save(camera_fb_t*fb,String&name,uint32_t epoch){
 if(!available)return false;name=newName("jpg",epoch);if(name.isEmpty())return false;String dest=path(name,"photos"),temp=dest+".part";
 if(SD_MMC.exists(dest)||SD_MMC.exists(temp)){fail("File name collision; photo was not overwritten");return false;}
 File f=SD_MMC.open(temp,FILE_WRITE);bool ok=f&&f.write(fb->buf,fb->len)==fb->len;if(f){f.flush();ok=ok&&!f.getWriteError();f.close();}
 if(ok){File check=SD_MMC.open(temp,FILE_READ);ok=check&&check.size()==fb->len;check.close();}
 if(!ok||!SD_MMC.rename(temp,dest)){fail("Photo could not be saved. Check the memory card.");return false;}
 refresh();return true;
}
bool list(const String&kind,uint32_t before,JsonObject out,uint32_t from,uint32_t to){
 if(kind!="videos"&&kind!="photos"&&kind!="legacy")return false;
 struct Entry{char name[32];uint32_t id,size;};Entry entries[PAGE_SIZE]={};size_t count=0,scanned=0;bool truncated=false;
 String dirPath=kind=="legacy"?"/snapshots":String("/robetix/")+kind;File dir=SD_MMC.open(dirPath);
 auto rows=out.createNestedArray("files");if(!dir){if(kind=="legacy")return true;return false;}
 uint32_t legacyIndex=0;
 while(File f=dir.openNextFile()){
  String n=baseName(f.name());bool valid=!f.isDirectory()&&validName(n,kind);uint32_t size=f.size();f.close();
  if(++scanned>SCAN_LIMIT){truncated=true;break;}if(!(scanned%32))delay(1);
  uint32_t id=kind=="legacy"?++legacyIndex:(valid?sequence(n):0);if(!valid||id>=before)continue;
  if(from){uint32_t created=kind=="legacy"?0:strtoul(n.c_str()+14,nullptr,10);if(created<from||created>to)continue;}
  size_t at=0;while(at<count&&entries[at].id>id)at++;if(at>=PAGE_SIZE)continue;
  for(size_t j=min(count,PAGE_SIZE-1);j>at;j--)entries[j]=entries[j-1];strlcpy(entries[at].name,n.c_str(),32);entries[at].id=id;entries[at].size=size;if(count<PAGE_SIZE)count++;
 }
 dir.close();
 for(size_t i=0;i<count;i++){
  auto row=rows.createNestedObject();row["name"]=entries[i].name;row["kind"]=kind;row["size"]=entries[i].size;row["epoch"]=kind=="legacy"?0:strtoul(entries[i].name+14,nullptr,10);
  if(kind=="videos"){uint8_t h[Avi::HEADER_SIZE];File f=SD_MMC.open(path(entries[i].name,kind));if(f&&f.read(h,sizeof(h))==sizeof(h)&&!memcmp(h,"RIFF",4)&&!memcmp(h+8,"AVI ",4)){uint32_t rate=Avi::get32(h+132);row["duration"]=rate?double(Avi::get32(h+140))*Avi::get32(h+128)/rate:0;}f.close();}
 }
 out["next"]=count==PAGE_SIZE?entries[count-1].id:0;out["scan_limited"]=truncated;return true;
}
bool remove(const String&name,const String&kind){String p=path(name,kind);if(p.isEmpty()||!SD_MMC.exists(p))return false;bool ok=SD_MMC.remove(p);if(ok)refresh();else fail("Could not delete file");return ok;}
void status(JsonObject o){if(!statsMutex){o["available"]=false;return;}xSemaphoreTake(statsMutex,portMAX_DELAY);o["available"]=available.load();o["total"]=total;o["used"]=used;o["free"]=total>used?total-used:0;o["error"]=lastError;o["unfinished_at_boot"]=unfinished;o["mode"]="1-bit";o["mount_attempts"]=mountAttempts;o["frequency_khz"]=mountKHz;o["card_type"]=cardType;o["mount_succeeded"]=mounted;xSemaphoreGive(statsMutex);}
}

