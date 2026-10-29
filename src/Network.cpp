#include "Network.h"
#include "NetworkPolicy.h"
#include "Settings.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <time.h>
namespace Network {
struct Config{String ssid,password,apSsid,apPassword;bool hotspot=true,ready=false,valid=true;};
static Config config;static SemaphoreHandle_t configMutex;static NetworkPolicy retry;
std::atomic<uint32_t> reconnects{0};static std::atomic<uint32_t> applyAt{0};
static std::atomic<bool> routerEnabled{false},associated{false};static std::atomic<uint16_t> reason{0};
static DNSServer dns;static bool dnsActive=false,ntp=false;static std::atomic<bool> mdns{false},attempting{false};static uint32_t nextMdns=0;
static std::atomic<bool> nameConflict{false};
static Config copy(){xSemaphoreTake(configMutex,portMAX_DELAY);Config c=config;xSemaphoreGive(configMutex);return c;}
static bool text(const String&s,size_t max){if(s.length()>max)return false;for(size_t i=0;i<s.length();i++)if((uint8_t)s[i]<32||s[i]==127)return false;return true;}
static bool wifiPassword(const String&s){if(s.isEmpty())return true;if(!text(s,64)||s.length()<8)return false;if(s.length()<64)return true;for(auto ch:s)if(!isxdigit((unsigned char)ch))return false;return true;}
bool connected(){return routerEnabled&&WiFi.status()==WL_CONNECTED&&uint32_t(WiFi.localIP())!=0;}
void changed(){applyAt=(millis()+2000)|1U;}
bool configure(JsonObject o,String&error,bool&changedConfig){
 if(!configMutex){error="Wi-Fi settings are unavailable";return false;}Config c=copy();
 if(!o["hotspot"].is<bool>()){error="Invalid network settings";return false;}bool nextHotspot=o["hotspot"];
 String ssid=o["ssid"]|"",password=o["password"]|"",ap=o["ap_ssid"]|"",apPass=o["ap_password"]|"";
 if(o["keep_password"]|false){if(ssid!=c.ssid){error="Enter the password for the new Wi-Fi";return false;}password=c.password;}
 if(ap.isEmpty())ap=c.apSsid;if(apPass.isEmpty())apPass=(ap!=c.apSsid&&c.apPassword==hotspotPasswordFor(c.apSsid))?hotspotPasswordFor(ap):c.apPassword;
 if(ap.isEmpty()||!text(ap,32)||apPass.length()<8||!text(apPass,63)){error="Choose a hotspot name (1–32 bytes) and your own hotspot password (8–63 characters)";return false;}
 if(!nextHotspot&&(ssid.isEmpty()||!text(ssid,32)||!wifiPassword(password))){error="Check SSID and Wi-Fi password lengths";return false;}
 if(!nextHotspot){c.ssid=ssid;c.password=password;}if(c.apSsid!=ap)nameConflict=false;c.apSsid=ap;c.apPassword=apPass;c.hotspot=nextHotspot;c.ready=true;c.valid=true;
 StaticJsonDocument<1024>d;d["version"]=1;d["hotspot_rule"]=1;d["ready"]=c.ready;d["ssid"]=c.ssid;d["password"]=c.password;d["ap_ssid"]=c.apSsid;d["ap_password"]=c.apPassword;d["hotspot"]=c.hotspot;String saved;serializeJson(d,saved);
 xSemaphoreTake(configMutex,portMAX_DELAY);changedConfig=prefs.getString("network","")!=saved;
 // One NVS value is committed atomically; a failed write cannot mix SSIDs/passwords.
 bool ok=!d.overflowed()&&(!changedConfig||prefs.putString("network",saved)>0)&&prefs.getString("network","")==saved;if(ok)config=c;xSemaphoreGive(configMutex);
 if(!ok)error="Could not save network settings";return ok;
}
static void apply(){Config c=copy();routerEnabled=c.valid&&c.ready&&!c.hotspot&&!c.ssid.isEmpty();associated=false;reason=0;ntp=false;
 WiFi.disconnect(false,false);if(mdns){MDNS.end();mdns=false;}dns.stop();dnsActive=false;
 if(WiFi.softAPSSID()!=c.apSsid)WiFi.softAPdisconnect(false);if(!WiFi.softAP(c.apSsid.c_str(),c.apPassword.c_str(),1,false,2))Serial.println("AP start failed");
 if(!c.ready)dnsActive=dns.start(53,"*",WiFi.softAPIP());retry.reset(millis());attempting=false;nextMdns=0;
 Serial.printf("Camera setup: http://%s (router IP is shown after DHCP)\n",WiFi.softAPIP().toString().c_str());
}
void begin(){configMutex=xSemaphoreCreateMutex();if(!configMutex)return;config.apSsid=prefs.getString("apSsid",setupSsid);config.apPassword=prefs.getString("apPass",setupPassword);config.ssid=prefs.getString("ssid","");config.password=prefs.getString("wifiPass","");config.hotspot=prefs.getBool("hotspot",true);config.ready=prefs.getBool("netReady",false);
 String saved=prefs.getString("network","");if(saved.length()){StaticJsonDocument<1024>d;if(deserializeJson(d,saved)||d["version"]!=1||!d["hotspot"].is<bool>()||!d["ap_ssid"].is<const char*>()||!d["ap_password"].is<const char*>()){config.valid=false;config.ready=false;}else{config.ssid=d["ssid"]|"";config.password=d["password"]|"";config.apSsid=d["ap_ssid"]|setupSsid;config.apPassword=d["ap_password"]|setupPassword;config.hotspot=d["hotspot"];config.ready=d["ready"]|true;}}
 // Apply the requested password rule once to already-installed camera hotspots.
 // Router credentials and setup progress are preserved in the same atomic value.
 bool migrate=true;if(saved.length()){StaticJsonDocument<1024>old;if(!deserializeJson(old,saved)&&old["hotspot_rule"]==1)migrate=false;}
 if(migrate&&config.valid){String next=hotspotPasswordFor(config.apSsid);if(!next.isEmpty()){
  StaticJsonDocument<1024>d;d["version"]=1;d["hotspot_rule"]=1;d["ready"]=config.ready;d["ssid"]=config.ssid;d["password"]=config.password;d["ap_ssid"]=config.apSsid;d["ap_password"]=next;d["hotspot"]=config.hotspot;String updated;serializeJson(d,updated);
  if(!d.overflowed()&&prefs.putString("network",updated)>0&&prefs.getString("network","")==updated)config.apPassword=next;else config.valid=false;
 }}
 if(config.apSsid.isEmpty()||!text(config.apSsid,32)||config.apPassword.length()<8||!text(config.apPassword,63)){config.apSsid=setupSsid;config.apPassword=setupPassword;config.valid=false;config.ready=false;}if(!config.hotspot&&(config.ssid.isEmpty()||!text(config.ssid,32)||!wifiPassword(config.password))){config.valid=false;config.ready=false;}
 WiFi.persistent(false);WiFi.mode(WIFI_AP_STA);WiFi.setHostname("robetix");WiFi.setAutoReconnect(false);WiFi.setSleep(false);
 // A short display name cannot be globally unique. Report a nearby collision
 // without silently changing an already printed setup label.
 int nearby=WiFi.scanNetworks(false,true,false,120);for(int i=0;i<nearby;i++)if(WiFi.SSID(i)==config.apSsid)nameConflict=true;WiFi.scanDelete();
 WiFi.onEvent([](WiFiEvent_t event,WiFiEventInfo_t info){if(event==ARDUINO_EVENT_WIFI_STA_CONNECTED){associated=true;reason=0;}else if(event==ARDUINO_EVENT_WIFI_STA_DISCONNECTED){associated=false;reason=info.wifi_sta_disconnected.reason;}});
 apply();
}
void tick(){if(!configMutex)return;if(dnsActive)dns.processNextRequest();uint32_t now=millis(),when=applyAt.load();if(when){if(!NetworkPolicy::due(now,when))return;if(applyAt.compare_exchange_strong(when,0))apply();}
 if(!routerEnabled)return;if(connected()){retry.connected(now);attempting=false;reason=0;if(!ntp){configTime(0,0,"pool.ntp.org","time.nist.gov");ntp=true;}if(!mdns&&NetworkPolicy::due(now,nextMdns)){nextMdns=now+15000;mdns=MDNS.begin("robetix");if(mdns)MDNS.addService("http","tcp",80);Serial.printf("Router assigned camera IP: http://%s\n",WiFi.localIP().toString().c_str());}return;}
 if(mdns){MDNS.end();mdns=false;}if(retry.timedOut(now)){WiFi.disconnect(false,false);retry.failed(now);attempting=false;return;}
 if(retry.ready(now)){Config c=copy();reason=0;associated=false;retry.started(now);attempting=true;WiFi.begin(c.ssid.c_str(),c.password.c_str());reconnects++;}
}
void status(JsonObject o){if(!configMutex){o["phase"]="unavailable";return;}Config c=copy();bool online=connected()&&!applyAt;uint16_t why=reason.load();
 o["hotspot_name_conflict"]=nameConflict.load();
 const char*phase=online?"connected":applyAt?"applying":!c.valid?"settings_error":!routerEnabled?"local_only":associated?"waiting_ip":why==WIFI_REASON_AUTH_FAIL||why==WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT||why==WIFI_REASON_HANDSHAKE_TIMEOUT?"check_password":why==WIFI_REASON_NO_AP_FOUND?"router_not_found":attempting?"connecting":"retry_wait";
 const char*message=online?"Connected to home Wi-Fi":!c.valid?"Saved Wi-Fi settings need attention. Run Wi-Fi Setup.":!routerEnabled?"Local viewing is ready. Remote viewing needs home Wi-Fi.":associated?"Connected to router. Waiting for an IP address.":!strcmp(phase,"check_password")?"Check the home Wi-Fi password and router security settings.":!strcmp(phase,"router_not_found")?"Home Wi-Fi was not found. Use a nearby 2.4 GHz network.":"Connecting to home Wi-Fi. The camera hotspot stays available.";
 o["gateway"]=WiFi.gatewayIP().toString();o["subnet"]=WiFi.subnetMask().toString();o["dns"]=WiFi.dnsIP().toString();o["channel"]=WiFi.channel();o["bssid"]=WiFi.BSSIDstr();o["rssi"]=online?WiFi.RSSI():0;
 o["phase"]=phase;o["message"]=message;o["reason"]=why;o["connected"]=online;o["ip"]=online?WiFi.localIP().toString():String("");o["ap_ip"]=WiFi.softAPIP().toString();o["ap_ssid"]=WiFi.softAPSSID();o["ssid"]=c.ssid;o["hotspot"]=c.hotspot;o["needs_network_setup"]=!c.ready;o["password_saved"]=c.password.length()>0;o["mdns"]=mdns.load();
}
}
