#include "Network.h"
#include "Settings.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <atomic>
#include <time.h>
namespace Network {
std::atomic<uint32_t> reconnects{0}; static uint32_t nextTry=0,backoff=10000,nextMdns=0; static bool mdns=false;
static std::atomic<uint32_t> applyAt{0};
static bool useRouter=false;
static bool ntpStarted=false;
static String routerSsid,routerPassword;
void changed(){applyAt=(millis()+2000)|1U;}
static void apply() {
  WiFi.disconnect();
  if(mdns){MDNS.end();mdns=false;}
  bool configured=prefs.getBool("netReady",false);
  useRouter=configured&&!prefs.getBool("hotspot",true);
  routerSsid=prefs.getString("ssid","");routerPassword=prefs.getString("wifiPass","");
  String name=configured?prefs.getString("apSsid","Robetix-Vision"):String(SETUP_AP_SSID);
  String password=configured?prefs.getString("apPass",""):String(SETUP_AP_PASSWORD);
  WiFi.softAPdisconnect(false);
  if(!WiFi.softAP(name.c_str(),password.c_str(),1,false,2)) Serial.println("AP start failed");
  Serial.printf("AP: %s URL http://192.168.4.1 (configure credentials in browser)\n",name.c_str());
  nextTry=millis();backoff=10000;nextMdns=0;
}
void begin() {
  WiFi.persistent(false); WiFi.mode(WIFI_AP_STA); WiFi.setHostname("robetix"); WiFi.setAutoReconnect(false);
  apply();
}
void tick() {
  uint32_t due=applyAt.load();
  if(due) {
    if((int32_t)(millis()-due)<0) return;
    if(applyAt.compare_exchange_strong(due,0)) apply();
  }
  if(!useRouter) return;
  if(WiFi.status()==WL_CONNECTED) {
    if(!ntpStarted){configTime(0,0,"pool.ntp.org","time.nist.gov");ntpStarted=true;}
    backoff=10000;
    if(!mdns && (int32_t)(millis()-nextMdns)>=0) { nextMdns=millis()+10000; mdns=MDNS.begin("robetix"); if(mdns) MDNS.addService("http","tcp",80); Serial.printf("Station IP %s\n",WiFi.localIP().toString().c_str()); }
    return;
  }
  if(mdns){MDNS.end();mdns=false;}
  if((int32_t)(millis()-nextTry)>=0) {
    if(routerSsid.length()) { WiFi.begin(routerSsid.c_str(),routerPassword.c_str()); reconnects++; }
    nextTry=millis()+backoff; backoff=min(backoff*2,uint32_t(120000));
  }
}
}
