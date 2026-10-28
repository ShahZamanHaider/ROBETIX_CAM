#pragma once
#include <stdint.h>
// UTC epoch + explicit fixed local offset. The browser refreshes the offset on save.
inline bool recordingWindow(uint32_t epoch,int offsetMinutes,unsigned start,unsigned end,uint8_t weekdays){
 if(epoch<1704067200UL)return false;
 int64_t local=int64_t(epoch)+offsetMinutes*60LL;int64_t day=local/86400;unsigned minute=(local%86400)/60;
 bool inside=start==end||(start<end?(minute>=start&&minute<end):(minute>=start||minute<end));
 if(start>end&&minute<end)day--;unsigned weekday=(day+4)%7;return inside&&(weekdays&(1U<<weekday));
}
