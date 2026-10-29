#include "Engine.h"
#include "Camera.h"
#include "Storage.h"
#include "Recording.h"
#include "Settings.h"
#include "Motion.h"
#include "Events.h"
#include "Cloud.h"
#include "Live.h"
#include "RemoteControl.h"
#include "Light.h"
#include "Maintenance.h"
#include "Schedule.h"
#include <esp_heap_caps.h>
#include <sys/time.h>
#include <atomic>
namespace Engine {
static QueueHandle_t commands;static SemaphoreHandle_t stateMutex,frameMutex;
static AppOptions opts;static uint8_t*frameBuffer=nullptr;static size_t capacity=0,frameLength=0;static uint32_t frameId=0,frameAt=0;
static std::atomic<uint32_t> previewUntil{0};static std::atomic<bool> recording{false},privacy{false},clockSet{false};
static char phase[16]="idle",message[160]="",lastFile[40]="";static uint32_t messageId=0,recordFrames=0,sdWriteUs=0,recordErrors=0,previewDrops=0;static std::atomic<uint32_t> recordAt{0};
static Recording recorder;static bool starting=false,cardOwned=false;static camera_status_t previous;static uint32_t nextFrameAt=0;
static uint32_t nextMotionAt=0,nextAutoAt=0,motionEvent=0;static uint8_t recordOrigin=0;static bool bootOptions=true;
static uint32_t nextRecordAt=0;static char pendingPhotoId[33]={},pendingStartId[33]={};
static int remoteTier=-1;static bool idleLow=false;static camera_status_t viewingSettings;
static const char*optionKeys[]={"name","clip_seconds","reserve_mb","cleanup","recording_quality","recording_fps","preview_fps","motion","motion_photo","motion_record","sensitivity","motion_seconds","min_motion_seconds","cooldown_seconds","quality_mode","recording_mode","zone_x","zone_y","zone_w","zone_h","retain_videos","retain_photos","video_days","photo_days","schedule","schedule_start","schedule_end","schedule_days","timezone_minutes","timelapse_seconds","motion_interval","motion_threshold","motion_cells","motion_neighbors","smaller_movements"};
static AppOptions mergeOptions(const AppOptions&old,const AppOptions&v,uint64_t mask){AppOptions out=old;
 if(mask&1)strlcpy(out.name,v.name,sizeof(out.name));
 #define MERGE(bit,field) if(mask&(1ULL<<bit))out.field=v.field;
 MERGE(1,clipSeconds) MERGE(2,reserveMB) MERGE(3,cleanup) MERGE(4,recordingQuality) MERGE(5,recordingFps) MERGE(6,previewFps) MERGE(7,motion) MERGE(8,motionPhoto) MERGE(9,motionRecord) MERGE(10,sensitivity) MERGE(11,motionSeconds) MERGE(12,minMotionSeconds) MERGE(13,cooldownSeconds) MERGE(14,qualityMode) MERGE(15,recordingMode) MERGE(16,zoneX) MERGE(17,zoneY) MERGE(18,zoneW) MERGE(19,zoneH) MERGE(20,retainVideos) MERGE(21,retainPhotos) MERGE(22,videoDays) MERGE(23,photoDays)
 MERGE(24,schedule) MERGE(25,scheduleStart) MERGE(26,scheduleEnd) MERGE(27,scheduleDays) MERGE(28,timezoneMinutes) MERGE(29,timelapseSeconds) MERGE(30,motionInterval) MERGE(31,motionThreshold) MERGE(32,motionCells) MERGE(33,motionNeighbors) MERGE(34,smallerMovements)
 #undef MERGE
 return out;
}
static void say(const char*s){xSemaphoreTake(stateMutex,portMAX_DELAY);strlcpy(message,s,sizeof(message));messageId++;xSemaphoreGive(stateMutex);}
static void setPhase(const char*s){xSemaphoreTake(stateMutex,portMAX_DELAY);strlcpy(phase,s,sizeof(phase));xSemaphoreGive(stateMutex);}
bool occupied(){return recording.load();}bool paused(){return privacy.load();}
size_t frameCapacity(){return capacity;}
uint32_t epoch(){time_t now=time(nullptr);return now>=1704067200LL&&now<=4102444800LL?uint32_t(now):0;}
bool setTime(uint32_t seconds){if(seconds<1704067200UL||seconds>4102444800UL)return false;timeval t={time_t(seconds),0};if(settimeofday(&t,nullptr))return false;clockSet=true;return true;}
static void encodeOptions(JsonObject o,const AppOptions&v){
 o["name"]=v.name;o["clip_seconds"]=v.clipSeconds;o["reserve_mb"]=v.reserveMB;o["cleanup"]=v.cleanup;o["recording_quality"]=v.recordingQuality;o["recording_fps"]=v.recordingFps;o["preview_fps"]=v.previewFps;o["privacy"]=v.privacy;
 o["motion"]=v.motion;o["motion_photo"]=v.motionPhoto;o["motion_record"]=v.motionRecord;o["sensitivity"]=v.sensitivity;o["motion_seconds"]=v.motionSeconds;o["min_motion_seconds"]=v.minMotionSeconds;o["cooldown_seconds"]=v.cooldownSeconds;
 o["quality_mode"]=v.qualityMode;o["recording_mode"]=v.recordingMode;o["zone_x"]=v.zoneX;o["zone_y"]=v.zoneY;o["zone_w"]=v.zoneW;o["zone_h"]=v.zoneH;o["retain_videos"]=v.retainVideos;o["retain_photos"]=v.retainPhotos;o["video_days"]=v.videoDays;o["photo_days"]=v.photoDays;
 o["motion_v2"]=true;o["smaller_movements"]=v.smallerMovements;o["motion_interval"]=v.motionInterval;o["motion_threshold"]=v.motionThreshold;o["motion_cells"]=v.motionCells;o["motion_neighbors"]=v.motionNeighbors;
 o["schedule"]=v.schedule;o["schedule_start"]=v.scheduleStart;o["schedule_end"]=v.scheduleEnd;o["schedule_days"]=v.scheduleDays;o["timezone_minutes"]=v.timezoneMinutes;o["timelapse_seconds"]=v.timelapseSeconds;
}
String cameraName(){xSemaphoreTake(stateMutex,portMAX_DELAY);String name=opts.name;xSemaphoreGive(stateMutex);return name;}
void options(JsonObject o){xSemaphoreTake(stateMutex,portMAX_DELAY);encodeOptions(o,opts);xSemaphoreGive(stateMutex);}
bool configure(JsonObject o,String&err,const char*remoteId){
 xSemaphoreTake(stateMutex,portMAX_DELAY);AppOptions v=opts;xSemaphoreGive(stateMutex);
 if(o.containsKey("name")){if(!o["name"].is<const char*>()||strlen(o["name"])<1||strlen(o["name"])>48){err="Choose a camera name (1–48 bytes)";return false;}strlcpy(v.name,o["name"],sizeof(v.name));}
 #define INT_FIELD(key,field,lo,hi) if(o.containsKey(key)){if(!o[key].is<int>()||o[key].as<int>()<lo||o[key].as<int>()>hi){err="Invalid " key;return false;}v.field=o[key].as<int>();}
 #define BOOL_FIELD(key,field) if(o.containsKey(key)){if(!o[key].is<bool>()){err="Invalid " key;return false;}v.field=o[key].as<bool>();}
 INT_FIELD("clip_seconds",clipSeconds,5,300) INT_FIELD("reserve_mb",reserveMB,16,1024) INT_FIELD("recording_quality",recordingQuality,0,2) INT_FIELD("recording_fps",recordingFps,1,10) INT_FIELD("preview_fps",previewFps,1,10)
 INT_FIELD("motion_interval",motionInterval,250,2000) INT_FIELD("motion_threshold",motionThreshold,6,40) INT_FIELD("motion_cells",motionCells,3,32) INT_FIELD("motion_neighbors",motionNeighbors,1,4)
 INT_FIELD("sensitivity",sensitivity,0,3) INT_FIELD("motion_seconds",motionSeconds,5,60) INT_FIELD("min_motion_seconds",minMotionSeconds,1,5) INT_FIELD("cooldown_seconds",cooldownSeconds,1,600)
 BOOL_FIELD("smaller_movements",smallerMovements) BOOL_FIELD("cleanup",cleanup) BOOL_FIELD("motion",motion) BOOL_FIELD("motion_photo",motionPhoto) BOOL_FIELD("motion_record",motionRecord)
 INT_FIELD("quality_mode",qualityMode,0,3) INT_FIELD("recording_mode",recordingMode,0,2)
 INT_FIELD("zone_x",zoneX,0,900) INT_FIELD("zone_y",zoneY,0,900) INT_FIELD("zone_w",zoneW,100,1000) INT_FIELD("zone_h",zoneH,100,1000)
 INT_FIELD("video_days",videoDays,1,3650) INT_FIELD("photo_days",photoDays,1,3650) BOOL_FIELD("retain_videos",retainVideos) BOOL_FIELD("retain_photos",retainPhotos)
 BOOL_FIELD("schedule",schedule) INT_FIELD("schedule_start",scheduleStart,0,1439) INT_FIELD("schedule_end",scheduleEnd,0,1439) INT_FIELD("schedule_days",scheduleDays,1,127) INT_FIELD("timezone_minutes",timezoneMinutes,-840,840) INT_FIELD("timelapse_seconds",timelapseSeconds,0,60)
 #undef INT_FIELD
 #undef BOOL_FIELD
 if(v.zoneX+v.zoneW>1000||v.zoneY+v.zoneH>1000){err="Motion area exceeds the image";return false;}
 if(o.containsKey("motion_record")&&!o.containsKey("recording_mode"))v.recordingMode=v.motionRecord?2:0;
 v.motionRecord=v.recordingMode==2;
 if(v.timelapseSeconds&&(v.recordingMode==2||v.clipSeconds<v.timelapseSeconds*2)){err="Time-lapse needs a clip at least twice its interval and cannot use movement recording";return false;}
 Command c;c.action=Action::Options;if(remoteId)strlcpy(c.remoteId,remoteId,sizeof(c.remoteId));c.options=v;c.mask=0;for(unsigned i=0;i<sizeof(optionKeys)/sizeof(*optionKeys);i++)if(o.containsKey(optionKeys[i]))c.mask|=1ULL<<i;
 if(o.containsKey("motion_record")||o.containsKey("recording_mode"))c.mask|=(1ULL<<9)|(1ULL<<15);
 if(!submit(c)){err="Camera busy; try again";return false;}return true;
}
bool submit(const Command&c){return commands&&xQueueSend(commands,&c,0)==pdTRUE;}
void requestPreview(){previewUntil=millis()+2500;}
bool copyFrame(uint8_t*out,size_t max,size_t&length,uint32_t&id,uint32_t&age,uint32_t after){
 if(!frameMutex||!out||paused())return false;if(xSemaphoreTake(frameMutex,pdMS_TO_TICKS(30))!=pdTRUE)return false;
 age=millis()-frameAt;bool ok=frameLength&&frameLength<=max&&age<1500&&frameId!=after;if(ok){memcpy(out,frameBuffer,frameLength);length=frameLength;id=frameId;}xSemaphoreGive(frameMutex);return ok;
}
static void publish(camera_fb_t*f){
 if(!frameBuffer||f->len>capacity){xSemaphoreTake(stateMutex,portMAX_DELAY);previewDrops++;xSemaphoreGive(stateMutex);return;}
 if(xSemaphoreTake(frameMutex,pdMS_TO_TICKS(10))==pdTRUE){memcpy(frameBuffer,f->buf,f->len);frameLength=f->len;frameAt=millis();frameId++;xSemaphoreGive(frameMutex);}
}
static void clearFrame(){xSemaphoreTake(frameMutex,portMAX_DELAY);frameLength=0;xSemaphoreGive(frameMutex);}
static bool saveOptions(){StaticJsonDocument<2048>d;encodeOptions(d.to<JsonObject>(),opts);String s;serializeJson(d,s);if(prefs.getString("appOptions","")==s)return true;return prefs.putString("appOptions",s)>0;}
static bool applyProfile(){
 if(!Camera::ready)return false;
 if(opts.qualityMode==3){auto s=esp_camera_sensor_get()->status;return Camera::configure(psramFound()?prefs.getInt("size",FRAMESIZE_VGA):FRAMESIZE_QVGA,prefs.getInt("quality",18),s.brightness,s.contrast,s.saturation,s.hmirror,s.vflip,false);}
 auto s=esp_camera_sensor_get()->status;const int sizes[]={FRAMESIZE_QVGA,FRAMESIZE_VGA,FRAMESIZE_SVGA},quality[]={24,18,14};
 bool ok=Camera::configure(psramFound()?sizes[opts.qualityMode]:FRAMESIZE_QVGA,quality[opts.qualityMode],s.brightness,s.contrast,s.saturation,s.hmirror,s.vflip,true);
 if(ok){Camera::extras(prefs.getInt("wb",0),prefs.getInt("exposure",-1),prefs.getInt("gain",-1),false);xSemaphoreTake(stateMutex,portMAX_DELAY);opts.previewFps=opts.qualityMode==0?3:opts.qualityMode==1?5:3;xSemaphoreGive(stateMutex);}return ok;
}
static void restoreIdle(){if(!idleLow)return;if(Camera::ready){auto&s=viewingSettings;Camera::configure(s.framesize,s.quality,s.brightness,s.contrast,s.saturation,s.hmirror,s.vflip,false);Camera::extras(s.wb_mode,s.aec?-1:s.aec_value,s.agc?-1:s.agc_gain,false);}idleLow=false;remoteTier=-1;clearFrame();Motion::rebase();}
static void restoreCamera(){if(Camera::ready){Camera::configure(previous.framesize,previous.quality,previous.brightness,previous.contrast,previous.saturation,previous.hmirror,previous.vflip,false);Camera::extras(previous.wb_mode,previous.aec?-1:previous.aec_value,previous.agc?-1:previous.agc_gain,false);}clearFrame();Motion::rebase();}
static void stopRecording(){
 if(!recording)return;setPhase("saving");bool ok=recorder.active&&recorder.finish(opts.timelapseSeconds>0);
 Events::complete(recorder.name,ok,cardOwned);
 if(!ok){xSemaphoreTake(stateMutex,portMAX_DELAY);recordErrors++;xSemaphoreGive(stateMutex);}
 xSemaphoreTake(Camera::mutex,portMAX_DELAY);restoreCamera();xSemaphoreGive(Camera::mutex);
 starting=false;recording=false;recordOrigin=0;if(cardOwned){xSemaphoreGive(Storage::operationMutex);cardOwned=false;}setPhase("idle");say(ok?"Video saved":"Video could not be finalized. Check the memory card.");
}
static void startRecording(uint8_t origin=0){
 if(recording){say("A recording is already running");return;}
 if(privacy||!Camera::ready||Camera::busy()){say("Resume the camera before recording");return;}
 if(!Storage::available||xSemaphoreTake(Storage::operationMutex,0)!=pdTRUE){say("Memory card unavailable or busy");return;}
 cardOwned=true;
 if(!Storage::reserve(2*1024*1024,opts.reserveMB,opts.cleanup)){xSemaphoreGive(Storage::operationMutex);cardOwned=false;say("Not enough memory card space");return;}
 xSemaphoreTake(Camera::mutex,portMAX_DELAY);restoreIdle();previous=esp_camera_sensor_get()->status;
 const int sizes[]={FRAMESIZE_QVGA,FRAMESIZE_VGA,FRAMESIZE_SVGA},qualities[]={24,18,14};
 bool ok=Camera::configure(psramFound()?sizes[opts.recordingQuality]:FRAMESIZE_QVGA,qualities[opts.recordingQuality],previous.brightness,previous.contrast,previous.saturation,previous.hmirror,previous.vflip,false);
 if(ok)Camera::extras(previous.wb_mode,previous.aec?-1:previous.aec_value,previous.agc?-1:previous.agc_gain,false);
 xSemaphoreGive(Camera::mutex);clearFrame();Motion::rebase();
 if(!ok){xSemaphoreGive(Storage::operationMutex);cardOwned=false;say("Recording camera setup failed; restart the camera");return;}
 recording=true;starting=true;recordOrigin=origin;recordAt=millis();setPhase("starting");nextFrameAt=nextRecordAt=millis();say(opts.timelapseSeconds?"Starting time-lapse":"Starting recording");
}
static void task(void*){
 bool photoPending=false;uint32_t consecutiveFailures=0;
 for(;;){
  Command c;
  if(xQueueReceive(commands,&c,0)==pdTRUE){RemoteControl::Completion completed(c.remoteId);
   if(c.action==Action::Start){bool already=recording;startRecording();if(!already&&recording){strlcpy(pendingStartId,c.remoteId,sizeof(pendingStartId));completed.defer();}}
   else if(c.action==Action::Stop){uint32_t errors=recordErrors;stopRecording();completed.done(!recording&&recordErrors==errors,"Recording stopped and finalized");if(opts.recordingMode){xSemaphoreTake(stateMutex,portMAX_DELAY);opts.recordingMode=0;opts.motionRecord=false;saveOptions();xSemaphoreGive(stateMutex);say("Video stopped. Automatic recording is now Off.");}}
   else if(c.action==Action::Photo){if(photoPending) say("A photo is already being saved");else if(privacy||Camera::busy()||!Camera::ready)say("Camera is unavailable");else{photoPending=true;strlcpy(pendingPhotoId,c.remoteId,sizeof(pendingPhotoId));completed.defer();}}
   else if(c.action==Action::Options){if(recording||Camera::busy())say("Stop recording or wait for the benchmark before changing settings");else {
    c.options=mergeOptions(opts,c.options,c.mask);if(c.options.zoneX+c.options.zoneW>1000||c.options.zoneY+c.options.zoneH>1000||(c.options.timelapseSeconds&&(c.options.recordingMode==2||c.options.clipSeconds<c.options.timelapseSeconds*2))){say("Settings conflict; check the motion area or time-lapse interval");continue;}
    bool nameChanged=!bootOptions&&strcmp(opts.name,c.options.name);bool profile=bootOptions||opts.qualityMode!=c.options.qualityMode;bootOptions=false;
    xSemaphoreTake(Camera::mutex,portMAX_DELAY);restoreIdle();xSemaphoreGive(Camera::mutex);
    xSemaphoreTake(stateMutex,portMAX_DELAY);c.options.privacy=privacy.load();opts=c.options;xSemaphoreGive(stateMutex);
    bool ok=true;if(profile&&!privacy){xSemaphoreTake(Camera::mutex,portMAX_DELAY);ok=applyProfile();xSemaphoreGive(Camera::mutex);clearFrame();}
    xSemaphoreTake(stateMutex,portMAX_DELAY);ok=saveOptions()&&ok;xSemaphoreGive(stateMutex);if(nameChanged)Cloud::nameChanged();Maintenance::policy(opts);Motion::reset();nextMotionAt=millis();nextAutoAt=millis()+1000;say(ok?"Settings saved":"Settings saved, but camera adjustment failed");completed.done(ok,ok?"Settings saved":"Settings could not be applied");}}
   else if(c.action==Action::Camera){if(recording||privacy||Camera::busy())say("Stop recording and resume the camera before changing picture settings");else{xSemaphoreTake(Camera::mutex,portMAX_DELAY);restoreIdle();bool ok=Camera::configure(c.camera[0],c.camera[1],c.camera[2],c.camera[3],c.camera[4],c.camera[5],c.camera[6],true);if(ok)ok=Camera::extras(c.camera[7],c.camera[8],c.camera[9],true);xSemaphoreGive(Camera::mutex);if(ok){xSemaphoreTake(stateMutex,portMAX_DELAY);opts.qualityMode=3;saveOptions();xSemaphoreGive(stateMutex);}Motion::reset();clearFrame();say(ok?"Picture settings saved":"Picture settings could not be applied");completed.done(ok,ok?"Picture settings saved":"Picture settings could not be applied");}}
   else if(c.action==Action::Benchmark){if(recording||privacy)say("Stop recording and resume the camera first");else if(!Camera::startBenchmark())say("Benchmark unavailable");}
   else if(c.action==Action::Privacy){if(Camera::busy())say("Wait for the benchmark to finish");else{stopRecording();Light::set(0);xSemaphoreTake(Camera::mutex,portMAX_DELAY);restoreIdle();bool ok=Camera::pause(c.flag);if(ok&&!c.flag)applyProfile();xSemaphoreGive(Camera::mutex);privacy=c.flag;completed.done(ok,ok?(c.flag?"Camera paused":"Camera resumed"):"Camera resume failed");xSemaphoreTake(stateMutex,portMAX_DELAY);opts.privacy=c.flag;saveOptions();xSemaphoreGive(stateMutex);Motion::reset();clearFrame();say(!ok?"Camera resume failed; check the ribbon and power":c.flag?"Camera paused; recording and motion monitoring stopped":"Camera resumed");}}
   else if(c.action==Action::Light){if(!privacy){Light::set(c.camera[0]);completed.done(Light::value()==c.camera[0],"Flashlight updated");}else say("Resume the camera to use the flashlight");}
   else if(c.action==Action::ResetCamera){if(recording||Camera::busy())say("Stop recording and wait for the camera first");else{xSemaphoreTake(Camera::mutex,portMAX_DELAY);restoreIdle();bool ok=Camera::pause(false)&&Camera::configure(psramFound()?FRAMESIZE_VGA:FRAMESIZE_QVGA,18,0,0,0,0,0,true)&&Camera::extras(0,-1,-1,true);xSemaphoreGive(Camera::mutex);Light::set(0);privacy=false;xSemaphoreTake(stateMutex,portMAX_DELAY);opts.qualityMode=1;opts.previewFps=5;opts.privacy=false;saveOptions();xSemaphoreGive(stateMutex);clearFrame();Motion::reset();say(ok?"Camera defaults restored. Wi-Fi and saved files kept.":"Camera reset failed. Check the sensor and power.");}}
   else if(c.action==Action::FactoryReset){uint32_t errors=recordErrors;stopRecording();if(recordErrors!=errors){Serial.println("ROBETIX_RESET_ERROR:recording");say("Recording could not be finalized. Reset cancelled.");completed.done(false,"Recording could not be finalized. Reset cancelled.");}else{Maintenance::pauseForReset();bool revoked=Cloud::prepareReset();Cloud::halt();Light::set(0);if(factoryResetSettings(revoked)){say("Camera resetting");completed.done(true,"Camera resetting");completed.finish();delay(1500);ESP.restart();}else{Serial.println("ROBETIX_RESET_ERROR:settings");say("Could not clear settings");}}}
   else if(c.action==Action::Restart){stopRecording();say("Restarting camera");completed.done(true,"Camera restarting");completed.finish();delay(1500);ESP.restart();}
  }
  uint32_t previewDeadline=previewUntil.load();bool view=previewDeadline&&(int32_t)(previewDeadline-millis())>0;if(previewDeadline&&!view)previewUntil.compare_exchange_strong(previewDeadline,0);
  bool remoteView=Live::viewing(),remoteDue=Live::wanted();
  bool testingMotion=Motion::testing();bool scheduleOpen=!opts.schedule||recordingWindow(epoch(),opts.timezoneMinutes,opts.scheduleStart,opts.scheduleEnd,opts.scheduleDays);
  if(recording&&recordOrigin&& !scheduleOpen)stopRecording();
  if(!recording&&!privacy&&!Camera::busy()&&scheduleOpen&&(int32_t)(millis()-nextAutoAt)>=0&&(opts.recordingMode==1||(opts.recordingMode==2&&!testingMotion&&Motion::active(opts.motionSeconds*1000UL)))){nextAutoAt=millis()+30000;startRecording(opts.recordingMode);}
  bool monitor=Motion::available()&&(opts.motion||opts.recordingMode==2||testingMotion);
  if(!recording&&!privacy&&!Camera::busy()&&Camera::ready){
   if(idleLow&&(view||(!monitor&&!remoteView)||(remoteView&&remoteTier!=Live::profile())||(!remoteView&&remoteTier>=0))){xSemaphoreTake(Camera::mutex,portMAX_DELAY);restoreIdle();xSemaphoreGive(Camera::mutex);}
   else if((monitor||remoteView)&&!view&&!idleLow){xSemaphoreTake(Camera::mutex,portMAX_DELAY);viewingSettings=esp_camera_sensor_get()->status;auto&s=viewingSettings;remoteTier=remoteView?Live::profile():-1;framesize_t remoteSize=remoteTier<=0?FRAMESIZE_QVGA:remoteTier==1?FRAMESIZE_VGA:FRAMESIZE_SVGA;idleLow=Camera::configure(psramFound()?remoteSize:FRAMESIZE_QVGA,remoteTier<=0?24:20,s.brightness,s.contrast,s.saturation,s.hmirror,s.vflip,false);xSemaphoreGive(Camera::mutex);clearFrame();Motion::rebase();}
  }
  bool motionDue=monitor&&(int32_t)(millis()-nextMotionAt)>=0;
  bool recordDue=recording&&(int32_t)(millis()-nextRecordAt)>=0;
  if((privacy||!Camera::ready)&&*pendingPhotoId){RemoteControl::complete(pendingPhotoId,false,"Camera is unavailable");pendingPhotoId[0]=0;photoPending=false;}
  if(privacy||Camera::busy()||!Camera::ready||(!view&&!remoteDue&&!recordDue&&!photoPending&&!motionDue&&!Cloud::wantsTest())){if(recording&&millis()-recordAt>=opts.clipSeconds*1000UL){stopRecording();nextAutoAt=millis()+500;}delay(20);continue;}
  uint32_t now=millis();if((int32_t)(now-nextFrameAt)<0&&nextFrameAt-now<=1000){delay(5);continue;}nextFrameAt=now+1000/(recording?opts.recordingFps:opts.previewFps);
  if(xSemaphoreTake(Camera::mutex,pdMS_TO_TICKS(100))!=pdTRUE){delay(5);continue;}
  camera_fb_t*f=Camera::capture();
  if(!f){xSemaphoreGive(Camera::mutex);if(++consecutiveFailures==3){photoPending=false;RemoteControl::complete(pendingPhotoId,false,"Camera capture failed");pendingPhotoId[0]=0;RemoteControl::complete(pendingStartId,false,"Camera capture failed");pendingStartId[0]=0;if(recording)stopRecording();say("Camera capture failed. Check power and camera connection.");}delay(500);continue;}consecutiveFailures=0;
  if(view)publish(f);
  bool writeFailed=false;
  if(starting){starting=false;if(!recorder.start(f,epoch(),opts.recordingFps))writeFailed=true;else{recordAt=millis();if(recordOrigin==2&&motionEvent)Events::attach(motionEvent,recorder.name,cardOwned);setPhase("recording");say("Recording started");}}
  if(recording&&!writeFailed&&recordDue){nextRecordAt=millis()+(opts.timelapseSeconds?opts.timelapseSeconds*1000UL:1000/opts.recordingFps);writeFailed=!recorder.append(f);xSemaphoreTake(stateMutex,portMAX_DELAY);recordFrames=recorder.frames;sdWriteUs=recorder.writeUs;strlcpy(lastFile,recorder.name.c_str(),sizeof(lastFile));xSemaphoreGive(stateMutex);}
  if(*pendingStartId){RemoteControl::complete(pendingStartId,!writeFailed&&recording,writeFailed?"Recording write failed":"Recording started",recorder.name.c_str());pendingStartId[0]=0;}
  if(photoPending){photoPending=false;bool own=cardOwned||(Storage::available&&xSemaphoreTake(Storage::operationMutex,0)==pdTRUE);String name;bool ok=own&&Storage::reserve(f->len,opts.reserveMB,false)&&Storage::save(f,name,epoch());if(own&&!cardOwned)xSemaphoreGive(Storage::operationMutex);say(ok?"Photo saved":"Photo could not be saved. Check card space or downloads.");RemoteControl::complete(pendingPhotoId,ok,ok?"Photo saved":"Photo could not be saved",ok?name.c_str():nullptr);pendingPhotoId[0]=0;}
  bool movement=false;
  if(motionDue){nextMotionAt=millis()+opts.motionInterval;if(!recording||recorder.writeUs<50000)movement=Motion::analyze(f,opts);if(testingMotion||(!opts.motion&&opts.recordingMode!=2))movement=false;}
  if(movement){
   String photo;bool own=cardOwned||(Storage::available&&xSemaphoreTake(Storage::operationMutex,0)==pdTRUE);
   bool saved=false;if(own&&opts.motionPhoto)saved=Storage::reserve(f->len,opts.reserveMB,false)&&Storage::save(f,photo,epoch());
   motionEvent=Events::add(epoch(),saved?photo:String(""),recorder.active?recorder.name:String(""),opts.motionPhoto&&!saved?"Movement detected; photo unavailable":"Movement detected",own,opts.name,uint8_t(min(uint32_t(100),Motion::score())));
   if(own&&!cardOwned)xSemaphoreGive(Storage::operationMutex);say("Movement detected");
  }
  if(movement||Cloud::wantsTest())Cloud::offer(f,movement?motionEvent:0,epoch(),opts.name,uint8_t(min(uint32_t(100),Motion::score())),opts.timezoneMinutes);
  Live::offer(f,recording&&sdWriteUs>50000);
  esp_camera_fb_return(f);xSemaphoreGive(Camera::mutex);
  if(movement&&!recording&&opts.recordingMode==2&&scheduleOpen){nextAutoAt=millis()+30000;startRecording(2);}
  if(recording){bool lowSpace=false;if(recorder.frames&&recorder.frames%25==0)lowSpace=!Storage::reserve(1024*1024,opts.reserveMB,false);bool motionEnded=recordOrigin==2&&millis()-recordAt>5000&&!Motion::active(opts.motionSeconds*1000UL);if(writeFailed||lowSpace||motionEnded||millis()-recordAt>=opts.clipSeconds*1000UL||recorder.frames>=Avi::MAX_FRAMES){stopRecording();nextAutoAt=millis()+(writeFailed||lowSpace?30000:500);if(writeFailed||lowSpace)say("Recording stopped: card write failed or space is low");}}
  delay(1);
 }
}
bool begin(){
 stateMutex=xSemaphoreCreateMutex();frameMutex=xSemaphoreCreateMutex();commands=xQueueCreate(4,sizeof(Command));if(!stateMutex||!frameMutex||!commands)return false;
 RemoteControl::begin();Light::begin();Motion::begin();Events::begin();Maintenance::begin();Cloud::begin();
 String saved=prefs.getString("appOptions","");if(saved.length()){StaticJsonDocument<2048>d;if(!deserializeJson(d,saved)){if(!d["motion_v2"].as<bool>()){d["cooldown_seconds"]=2;d["min_motion_seconds"]=1;}opts.privacy=d["privacy"]|false;privacy=opts.privacy;String err;configure(d.as<JsonObject>(),err);}}else{Command c;c.action=Action::Options;c.options=opts;submit(c);}
 if(privacy){xSemaphoreTake(Camera::mutex,portMAX_DELAY);Camera::pause(true);xSemaphoreGive(Camera::mutex);}
 capacity=psramFound()?192*1024:32*1024;frameBuffer=(uint8_t*)heap_caps_malloc(capacity,psramFound()?MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT:MALLOC_CAP_8BIT);
 if(!frameBuffer){capacity=0;say("Preview buffer unavailable");}
 bool ok=xTaskCreate(task,"capture",8192,nullptr,1,nullptr)==pdPASS;if(!ok){vQueueDelete(commands);commands=nullptr;}return ok;
}
void status(JsonObject o){if(!stateMutex){o["error"]="Capture service unavailable";return;}xSemaphoreTake(stateMutex,portMAX_DELAY);o["recording"]=recording.load();o["phase"]=phase;o["elapsed"]=recording?(millis()-recordAt)/1000:0;o["frames"]=recordFrames;o["write_ms"]=sdWriteUs/1000.0;o["record_errors"]=recordErrors;o["preview_drops"]=previewDrops;o["privacy"]=privacy.load();o["message"]=message;o["message_id"]=messageId;o["last_video"]=lastFile;o["clock_set"]=epoch()>0;o["light"]=Light::value();encodeOptions(o.createNestedObject("options"),opts);xSemaphoreGive(stateMutex);o["event_id"]=Events::latest();Motion::status(o.createNestedObject("motion"));}
}
