#pragma once
#include <stdint.h>
struct RecoveryWindow {
 int64_t until=0;uint8_t attempts=0;bool active=false;
 void start(int64_t now){until=now+300000000LL;attempts=0;active=true;}
 bool permit(int64_t now){if(!active||now>=until||attempts>=5)return false;attempts++;return true;}
 void finish(){active=false;until=0;}
};
