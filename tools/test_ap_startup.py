from pathlib import Path
import subprocess,argparse,tempfile
root=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--compiler',default='g++');args=p.parse_args();source=(root/'src/Network.cpp').read_text();a=source.index('static bool protectedStart(');b=source.index('static void apply()',a)
prefix=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
struct String{const char*value;const char*c_str()const{return value;}size_t length()const{return strlen(value);}bool isEmpty()const{return !*value;}};struct Config{String apSsid,apPassword;};using esp_err_t=int;enum{ESP_OK=0,ESP_ERR_WIFI_NOT_STARTED=7,WIFI_MODE_APSTA=3,WIFI_STORAGE_RAM=0,WIFI_IF_AP=1,WIFI_AUTH_WPA2_PSK=3};struct wifi_config_t{struct{uint8_t ssid[32],password[64];unsigned ssid_len,channel,authmode,max_connection,beacon_interval;}ap;};
static bool stopped=false,configured=false,started=false,failConfig=false;static unsigned order=0;
int esp_wifi_stop(){assert(order++==0);stopped=true;return ESP_OK;}int esp_wifi_set_mode(int mode){assert(stopped&&!started&&mode==WIFI_MODE_APSTA&&order++==1);return ESP_OK;}int esp_wifi_set_storage(int value){assert(value==WIFI_STORAGE_RAM&&order++==2);return ESP_OK;}int esp_wifi_set_config(int interface,const wifi_config_t*c){assert(interface==WIFI_IF_AP&&order++==3);assert(c->ap.authmode==WIFI_AUTH_WPA2_PSK&&c->ap.max_connection==2);assert(c->ap.ssid_len==17);assert(strlen((const char*)c->ap.password)==8);configured=!failConfig;return failConfig?-1:ESP_OK;}int esp_wifi_start(){assert(stopped&&configured&&order++==4);started=true;return ESP_OK;}
size_t strlcpy(char*d,const char*s,size_t n){size_t l=strlen(s);if(n){size_t c=l<n-1?l:n-1;memcpy(d,s,c);d[c]=0;}return l;}
'''
suffix=r'''
int main(){Config c{{"ROBETIX-VISION-27"},{"12345678"}};assert(protectedStart(c)&&started&&order==5);stopped=configured=started=false;order=0;failConfig=true;assert(!protectedStart(c)&&!started&&order==4);order=0;Config invalid{{""},{"short"}};assert(!protectedStart(invalid)&&!started&&order==1);puts("PASS: actual AP startup stops radio, configures WPA2 before starting, validates bounds and stays stopped on failure.");}
'''
with tempfile.TemporaryDirectory(prefix='rv-ap-')as temp:
 directory=Path(temp);cpp=directory/'ap.cpp';exe=directory/('ap.exe'if __import__('os').name=='nt'else'ap');cpp.write_text(prefix+source[a:b]+suffix)
 cmd=[args.compiler]+(['c++','-nostdlib++']if'zig'in Path(args.compiler).name else[])+['-std=c++17','-O1',str(cpp),'-o',str(exe)];subprocess.run(cmd,check=True);subprocess.run([str(exe)],check=True)
