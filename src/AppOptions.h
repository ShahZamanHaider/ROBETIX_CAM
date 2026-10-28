#pragma once
#include <stdint.h>
struct AppOptions {
 char name[49]="Front Door";
 uint16_t clipSeconds=60, reserveMB=64, cooldownSeconds=30, motionSeconds=15;
 uint8_t recordingQuality=1,recordingFps=5,previewFps=5,sensitivity=1,minMotionSeconds=2;
 uint16_t motionInterval=500;uint8_t motionThreshold=12,motionCells=4,motionNeighbors=1;
 uint8_t qualityMode=1,recordingMode=0;
 uint16_t zoneX=0,zoneY=0,zoneW=1000,zoneH=1000;
 uint16_t videoDays=15,photoDays=15;
 bool retainVideos=false,retainPhotos=false;
 bool schedule=false;uint16_t scheduleStart=1320,scheduleEnd=360;uint8_t scheduleDays=127,timelapseSeconds=0;int16_t timezoneMinutes=0;
 bool cleanup=false,motion=false,motionPhoto=true,motionRecord=false,privacy=false;
};
