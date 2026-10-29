#include "DeviceStatus.h"
#include "Camera.h"
#include "Motion.h"
#include "Network.h"
#include "Storage.h"
#include "Engine.h"
#include "Light.h"
#include "Cloud.h"
#include "Settings.h"
#include "FirmwareInfo.h"
#include <WiFi.h>
#include <esp_system.h>
namespace DeviceStatus {
void write(JsonObject d){
 FirmwareInfo::write(d.createNestedObject("firmware"));
 d["epoch"]=Engine::epoch();
 d["cloud_cleanup_pending"]=cloudCleanupPending;d["factory_label_numeric"]=factoryLabelNumeric;
 d["setup_complete"]=prefs.getBool("setupDone",true);d["reset_reason"]=esp_reset_reason();d["uptime"]=millis()/1000;d["heap"]=ESP.getFreeHeap();d["min_heap"]=ESP.getMinFreeHeap();d["psram"]=ESP.getPsramSize();d["free_psram"]=ESP.getFreePsram();
 JsonObject net=d.createNestedObject("network");Network::status(net);for(const char*k:{"ip","ap_ip","connected","hotspot","needs_network_setup","ap_ssid","ssid"})d[k]=net[k];d["rssi"]=Network::connected()?WiFi.RSSI():0;d["reconnects"]=Network::reconnects.load();
 if(Camera::mutex&&xSemaphoreTake(Camera::mutex,pdMS_TO_TICKS(50))==pdTRUE){Camera::status(d.createNestedObject("camera"));xSemaphoreGive(Camera::mutex);}
 Storage::status(d.createNestedObject("storage"));Light::status(d.createNestedObject("flashlight"));Engine::status(d.createNestedObject("app"));Cloud::status(d.createNestedObject("cloud"));
 auto caps=d.createNestedObject("capabilities");caps["protocol"]=2;caps["recording"]=bool(Storage::available);caps["files"]=bool(Storage::available);caps["photo"]=bool(Storage::available);caps["camera"]=bool(Camera::ready);caps["flashlight"]=true;caps["motion"]=Motion::available();caps["file_chunk_bytes"]=psramFound()?8192:1024;
}
}
