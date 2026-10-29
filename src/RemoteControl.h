#pragma once
#include <ArduinoJson.h>
namespace RemoteControl {
void begin();bool receive(JsonObject,String&);bool drain(String&);
void complete(const char*,bool,const char*,const char* file=nullptr);
struct Completion {
 char id[33]={};bool ok=false;const char*message="Camera rejected the command";
 explicit Completion(const char*value){if(value)strlcpy(id,value,sizeof(id));}
 void defer(){id[0]=0;}
 void done(bool success,const char*text){ok=success;message=text;}
 void finish(){if(id[0]){complete(id,ok,message);id[0]=0;}}
 ~Completion(){finish();}
};
}
