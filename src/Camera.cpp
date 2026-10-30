#include "Camera.h"
#include "Settings.h"
#include <atomic>
namespace Camera {
SemaphoreHandle_t mutex;
std::atomic<bool> ready{false}; uint16_t pid=0; String error;
uint32_t captured=0,failures=0,captureUs=0;static uint32_t rateAt=0,rateFrames=0,lastCaptureAt=0;static float captureRate=0;
static std::atomic<bool> benchmarking{false};
static portMUX_TYPE benchMux=portMUX_INITIALIZER_UNLOCKED;
struct Result { int size,quality; uint32_t frames,failed,ms,bytes,heap,psram; };
static Result results[12]; static int resultCount=0;
bool busy(){return benchmarking.load();}
static const camera_sensor_info_t*info=nullptr;
static bool nativeSensor(sensor_t*s){if(!s)return false;info=esp_camera_sensor_get_info(&s->id);pid=s->id.PID;return info&&info->support_jpeg&&(pid==OV2640_PID||pid==OV3660_PID||pid==OV5640_PID);}
static bool validSize(int n) { return (!info||n<=info->max_size)&&(n==FRAMESIZE_QVGA || n==FRAMESIZE_VGA || n==FRAMESIZE_SVGA || n==FRAMESIZE_XGA || n==FRAMESIZE_SXGA || n==FRAMESIZE_UXGA); }
bool configure(int size,int quality,int brightness,int contrast,int saturation,int mirror,int flip,bool persist) {
  if(!ready || !validSize(size) || (!psramFound() && size!=FRAMESIZE_QVGA) || quality<10 || quality>40 || abs(brightness)>2 || abs(contrast)>2 || abs(saturation)>2 || mirror<0 || mirror>1 || flip<0 || flip>1) return false;
  sensor_t *s=esp_camera_sensor_get();
   // Intentional size changes still reallocate the driver frame buffer. Unlike
   // the old implementation, failure restores the last working camera setup.
   // Automatic viewer transitions no longer request a size change (Engine.cpp).
  if(size!=s->status.framesize) {
    extern camera_config_t config;
     const camera_config_t previous=config;
     camera_config_t requested=previous;
     requested.frame_size=(framesize_t)size;
     requested.jpeg_quality=quality;
     esp_camera_deinit();
     ready=false;
     esp_err_t e=esp_camera_init(&requested);
     if(e!=ESP_OK){
       Serial.printf("ROBETIX_CAMERA: requested size %d failed 0x%x; restoring prior size %d\n",
                     size,unsigned(e),int(previous.frame_size));
       esp_camera_deinit();
       delay(80);
       esp_err_t restore=esp_camera_init(&previous);
       s=restore==ESP_OK?esp_camera_sensor_get():nullptr;
       if(s && nativeSensor(s)){
         ready=true;
         error="Picture size change failed; previous camera mode restored";
       }else{
         ready=false;
         error="Camera reinit and recovery failed; check power and reboot";
         Serial.printf("ROBETIX_CAMERA: restore failed 0x%x\n",unsigned(restore));
       }
       return false;
     }
    s=esp_camera_sensor_get();
     if(!s || !nativeSensor(s)){
       esp_camera_deinit();
       delay(80);
       esp_err_t restore=esp_camera_init(&previous);
       s=restore==ESP_OK?esp_camera_sensor_get():nullptr;
       ready=s && nativeSensor(s);
       error=ready?"Unsupported camera mode; previous mode restored":"Camera sensor recovery failed; reboot";
       return false;
     }
     config=requested;
     ready=true;
  }
  int rc=s->set_quality(s,quality) | s->set_brightness(s,brightness) | s->set_contrast(s,contrast) | s->set_saturation(s,saturation) | s->set_hmirror(s,mirror) | s->set_vflip(s,flip);
  if(rc) return false;
  error="";
  if(persist) {
    prefs.putInt("size",size); prefs.putInt("quality",quality); prefs.putInt("bright",brightness); prefs.putInt("contrast",contrast); prefs.putInt("sat",saturation); prefs.putInt("mirror",mirror); prefs.putInt("flip",flip);
    // Boot probes conservatively; the selected custom size is applied by Engine.
  }
  return true;
}
camera_config_t config={};
static bool suspended=false;
bool extras(int wb,int exposure,int gain,bool persist){
 if(!ready||wb<0||wb>4||exposure< -1||exposure>1200||gain< -1||gain>30)return false;
 auto s=esp_camera_sensor_get();if(pid!=OV2640_PID&&pid!=OV3660_PID&&pid!=OV5640_PID)return false;
 if(!s||!s->set_wb_mode||!s->set_exposure_ctrl||!s->set_gain_ctrl)return false;
 int rc=s->set_whitebal(s,1)|s->set_awb_gain(s,1)|s->set_wb_mode(s,wb)|s->set_exposure_ctrl(s,exposure<0)|s->set_gain_ctrl(s,gain<0);
 if(exposure>=0)rc|=s->set_aec_value(s,exposure);if(gain>=0)rc|=s->set_agc_gain(s,gain);
 if(!rc&&persist){prefs.putInt("wb",wb);prefs.putInt("exposure",exposure);prefs.putInt("gain",gain);}return rc==0;
}
bool pause(bool enabled){
 if(enabled){if(ready){esp_camera_deinit();ready=false;}pinMode(32,OUTPUT);digitalWrite(32,HIGH);suspended=true;return true;}
 if(!suspended&&ready)return true;
 esp_err_t e=esp_camera_init(&config);auto s=esp_camera_sensor_get();
 if(e!=ESP_OK||!s||!nativeSensor(s)){error="Camera restart failed: "+String(esp_err_to_name(e))+". Check ribbon, sensor and power.";ready=false;return false;}
 ready=true;suspended=false;error="";configure(config.frame_size,prefs.getInt("quality",18),prefs.getInt("bright",0),prefs.getInt("contrast",0),prefs.getInt("sat",0),prefs.getInt("mirror",0),prefs.getInt("flip",0),false);extras(prefs.getInt("wb",0),prefs.getInt("exposure",-1),prefs.getInt("gain",-1),false);return true;
}
bool begin() {
  mutex=xSemaphoreCreateMutex(); if(!mutex) { error="Camera mutex allocation failed"; return false; }
  config.pin_pwdn=32; config.pin_reset=-1; config.pin_xclk=0;
  config.pin_sccb_sda=26; config.pin_sccb_scl=27;
  config.pin_d7=35; config.pin_d6=34; config.pin_d5=39; config.pin_d4=36;
  config.pin_d3=21; config.pin_d2=19; config.pin_d1=18; config.pin_d0=5;
  config.pin_vsync=25; config.pin_href=23; config.pin_pclk=22;
  config.xclk_freq_hz=20000000; config.ledc_timer=LEDC_TIMER_0; config.ledc_channel=LEDC_CHANNEL_0;
  config.pixel_format=PIXFORMAT_JPEG; config.frame_size=psramFound()?FRAMESIZE_VGA:FRAMESIZE_QVGA;
  config.jpeg_quality=16; config.fb_count=1;
  config.fb_location=psramFound()?CAMERA_FB_IN_PSRAM:CAMERA_FB_IN_DRAM;
  config.grab_mode=CAMERA_GRAB_WHEN_EMPTY;
  esp_err_t e=esp_camera_init(&config);
  if(e!=ESP_OK){Serial.printf("Camera init error: %s (0x%x). Retrying native JPEG at QVGA.\n",esp_err_to_name(e),e);esp_camera_deinit();config.frame_size=FRAMESIZE_QVGA;config.jpeg_quality=20;delay(100);e=esp_camera_init(&config);}
  sensor_t *s=esp_camera_sensor_get();
  if(s) { pid=s->id.PID; Serial.printf("Sensor PID=0x%04x VER=0x%02x MID=%02x:%02x\n",pid,s->id.VER,s->id.MIDH,s->id.MIDL); }
  if(e==ESP_ERR_NOT_SUPPORTED){camera_config_t probe=config;probe.pixel_format=PIXFORMAT_RGB565;probe.frame_size=FRAMESIZE_QQVGA;esp_camera_deinit();if(esp_camera_init(&probe)==ESP_OK){auto detected=esp_camera_sensor_get();nativeSensor(detected);esp_camera_deinit();error="This camera module is not supported for native JPEG. Software conversion is not enabled.";return false;}}
  if(e!=ESP_OK) { error="Native JPEG init failed (0x"+String(e,HEX)+"). Check sensor probe, ribbon, native-JPEG support and power."; return false; }
  if(!s || !nativeSensor(s)) { esp_camera_deinit(); error="This camera module is not supported for native JPEG"; return false; }
  ready=true;
  configure(config.frame_size,prefs.getInt("quality",16),prefs.getInt("bright",0),prefs.getInt("contrast",0),prefs.getInt("sat",0),prefs.getInt("mirror",0),prefs.getInt("flip",0),false);
  extras(prefs.getInt("wb",0),prefs.getInt("exposure",-1),prefs.getInt("gain",-1),false);
  Serial.printf("Detected %s; native JPEG; PSRAM %u bytes\n",info?info->name:"unknown",ESP.getPsramSize()); return true;
}
camera_fb_t* capture() {
  if(!ready) return nullptr;
  uint32_t start=micros(); camera_fb_t *fb=esp_camera_fb_get(); captureUs=micros()-start;
  if(!fb || fb->format!=PIXFORMAT_JPEG || fb->len<4 || fb->buf[0]!=0xff || fb->buf[1]!=0xd8 || fb->buf[fb->len-2]!=0xff || fb->buf[fb->len-1]!=0xd9) {
    if(fb) esp_camera_fb_return(fb); failures++; return nullptr;
  }
  uint32_t now=millis();if(!lastCaptureAt||now-lastCaptureAt>2000){rateAt=now;rateFrames=0;captureRate=0;}lastCaptureAt=now;captured++;rateFrames++;if(now-rateAt>=1000){captureRate=rateFrames*1000.0/(now-rateAt);rateAt=now;rateFrames=0;}return fb;
}
void status(JsonObject o) {
  o["ready"]=ready.load(); o["pid"]=pid; o["error"]=error; o["format"]="native JPEG";o["model"]=info?info->name:"Unknown";o["native_jpeg"]=info&&info->support_jpeg;o["max_size"]=info?int(info->max_size):0;auto sizes=o.createNestedArray("sizes");for(int n:{5,8,9,10,12,13})if(validSize(n)&&(psramFound()||n==5))sizes.add(n);auto controls=o.createNestedObject("controls");bool supported=pid==OV2640_PID||pid==OV3660_PID||pid==OV5640_PID;for(const char*k:{"brightness","contrast","saturation","mirror","flip","white_balance","exposure","gain"})controls[k]=supported;
  o["capture_fps"]=millis()-lastCaptureAt<2000?captureRate:0;o["captured"]=captured; o["failures"]=failures; o["capture_ms"]=captureUs/1000.0;
  if(ready) { auto s=esp_camera_sensor_get(); o["size"]=s->status.framesize; o["quality"]=s->status.quality; o["brightness"]=s->status.brightness; o["contrast"]=s->status.contrast; o["saturation"]=s->status.saturation; o["mirror"]=s->status.hmirror; o["flip"]=s->status.vflip;o["white_balance"]=s->status.wb_mode;o["exposure"]=s->status.aec?-1:s->status.aec_value;o["gain"]=s->status.agc?-1:s->status.agc_gain; }
}
bool startBenchmark() {
  if(!ready || benchmarking.exchange(true)) return false;
  portENTER_CRITICAL(&benchMux); resultCount=0; portEXIT_CRITICAL(&benchMux);
  if(xTaskCreate(benchmarkTask,"benchmark",4096,nullptr,1,nullptr)!=pdPASS) { benchmarking=false; return false; } return true;
}
void benchmarkTask(void*) {
  xSemaphoreTake(mutex,portMAX_DELAY);
  auto s=esp_camera_sensor_get(); auto original=s->status;
  const int sizes[]={FRAMESIZE_QVGA,FRAMESIZE_VGA,FRAMESIZE_SVGA,FRAMESIZE_XGA,FRAMESIZE_SXGA,FRAMESIZE_UXGA};
  for(int size:sizes) { if(!psramFound() && size!=FRAMESIZE_QVGA) break;
    for(int quality:{12,20}) {
      Result r={}; r.size=size; r.quality=quality;
      if(!configure(size,quality,0,0,0,0,0,false)) { r.failed=1; }
      else {
        for(int i=0;i<3;i++) { auto f=capture(); if(f) esp_camera_fb_return(f); vTaskDelay(1); }
        uint32_t start=millis();
        for(int i=0;i<30;i++) { auto f=capture(); if(f) {r.frames++;r.bytes+=f->len;esp_camera_fb_return(f);} else r.failed++; vTaskDelay(1); }
        r.ms=millis()-start;
      }
      r.heap=ESP.getFreeHeap();r.psram=ESP.getFreePsram();
      portENTER_CRITICAL(&benchMux); results[resultCount++]=r; portEXIT_CRITICAL(&benchMux);
      if(!ready) break;
    } if(!ready) break;
  }
  if(ready) configure(original.framesize,original.quality,original.brightness,original.contrast,original.saturation,original.hmirror,original.vflip,false);
  xSemaphoreGive(mutex); benchmarking=false; vTaskDelete(nullptr);
}
void benchmarkStatus(JsonObject o) {
  Result copy[12]; int count;
  portENTER_CRITICAL(&benchMux); count=resultCount; memcpy(copy,results,sizeof(results)); portEXIT_CRITICAL(&benchMux);
  o["running"]=busy(); auto a=o.createNestedArray("results");
  for(int i=0;i<count;i++) { auto r=copy[i];auto x=a.createNestedObject();x["size"]=r.size;x["quality"]=r.quality;x["frames"]=r.frames;x["failures"]=r.failed;x["fps"]=r.ms?1000.0*r.frames/r.ms:0;x["mean_bytes"]=r.frames?r.bytes/r.frames:0;x["free_heap"]=r.heap;x["free_psram"]=r.psram; }
}
}
