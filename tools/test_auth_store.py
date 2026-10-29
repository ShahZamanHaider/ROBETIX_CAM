from pathlib import Path
import subprocess,argparse,tempfile
root=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--compiler',default='g++');a=p.parse_args()
source='\n'.join(line for line in (root/'src/Auth.cpp').read_text().splitlines()if not line.startswith('#include'))
prefix=r'''
#include <string>
#include <map>
#include <mutex>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "RecoveryWindow.h"
#include "LocalAccessWindow.h"
struct String:std::string{using std::string::string;String(const std::string&s):std::string(s){}bool isEmpty()const{return empty();}};
String operator+(const String&s,unsigned n){return String(std::string(s)+std::to_string(n));}String operator+(const String&s,int n){return s+unsigned(n);}
struct Preferences{std::map<std::string,String>data;bool fail=false;String getString(const char*k,const char*f){auto i=data.find(k);return i==data.end()?String(f):i->second;}size_t putString(const char*k,const String&v){if(fail)return 0;data[k]=v;return v.length();}bool getBool(const char*k,bool f){auto i=data.find(k);return i==data.end()?f:i->second=="1";}bool putBool(const char*k,bool v){data[k]=v?"1":"0";return true;}void remove(const char*k){data.erase(k);}}prefs;
using SemaphoreHandle_t=std::mutex*;static const int portMAX_DELAY=-1;static std::mutex lock;SemaphoreHandle_t xSemaphoreCreateMutex(){return &lock;}int xSemaphoreTake(SemaphoreHandle_t m,int wait){if(wait==0)return m->try_lock();m->lock();return 1;}void xSemaphoreGive(SemaphoreHandle_t m){m->unlock();}static const int pdTRUE=1;
static int64_t micros=1000;int64_t esp_timer_get_time(){return micros;}size_t strlcpy(char*d,const char*s,size_t n){size_t l=strlen(s);if(n){size_t copy=l<n-1?l:n-1;memcpy(d,s,copy);d[copy]=0;}return l;}
static bool shortToken=false,failHash=false;static unsigned serial=0;String adminHash="",authSalt="test-only-salt";String randomHex(size_t n){if(shortToken)return String("short");String value(n*2,'0');char end[17];snprintf(end,sizeof(end),"%016x",++serial);value.replace(value.size()-16,16,end);return value;}
String passwordHash(const String&v){if(failHash)return String("");uint64_t hash=1469598103934665603ULL;for(unsigned char c:std::string(authSalt)+v){hash^=c;hash*=1099511628211ULL;}String result;for(unsigned i=0;i<4;i++){char part[17];snprintf(part,sizeof(part),"%016llx",(unsigned long long)(hash+i));result+=part;}return result;}
bool setAdminPassword(const String&){return true;}
'''
suffix=r'''
int main(){prefs.data["cloud"]="preserved-fixture";Auth::begin();String first=Auth::issue(true);assert(Auth::valid(first));String link=Auth::handoff(first);size_t count=prefs.data.size();bool remember=false;String moved=Auth::acceptHandoff(link,remember);assert(remember&&moved==first&&prefs.data.size()==count);assert(Auth::acceptHandoff(link,remember).isEmpty());assert(Auth::valid(first));for(auto&d:Auth::devices)d=Auth::Device{};Auth::begin();assert(Auth::valid(first));Auth::revoke(moved);assert(!Auth::valid(first));
String current=Auth::issue(true);String expiring=Auth::handoff(current);micros+=61000000;Auth::tick();assert(!Auth::handoffToken[0]);assert(Auth::acceptHandoff(expiring,remember).isEmpty());assert(Auth::valid(current));Auth::revoke(current);String tokens[8];for(auto&i:tokens)i=Auth::issue(true);for(auto&i:tokens)assert(Auth::valid(i));String ninth=Auth::issue(true);assert(Auth::valid(ninth));unsigned valid=0;for(auto&i:tokens)valid+=Auth::valid(i);assert(valid==7);prefs.fail=true;assert(Auth::issue(true).isEmpty());assert(Auth::valid(ninth));prefs.fail=false;
size_t before=prefs.data.size();shortToken=true;assert(Auth::issue(true).isEmpty());shortToken=false;failHash=true;assert(Auth::issue(true).isEmpty());assert(!Auth::valid(ninth));failHash=false;assert(prefs.data.size()==before&&Auth::valid(ninth));String temporary=Auth::issue(false);assert(Auth::valid(temporary));micros+=12LL*60*60*1000000+1;assert(!Auth::valid(temporary));assert(Auth::valid(ninth));assert(prefs.data["cloud"]=="preserved-fixture");puts("PASS: actual Auth source, persistence across reboot, one-use same-device origin handoff, revocation, expiry cleanup, bounded slots and NVS failure.");}
'''
with tempfile.TemporaryDirectory(prefix='rv-auth-')as directory:
 path=Path(directory);cpp=path/'auth.cpp';exe=path/('auth.exe'if __import__('os').name=='nt'else'auth');cpp.write_text(prefix+source+suffix)
 cmd=[a.compiler]+(['c++']if'zig'in Path(a.compiler).name else[])+['-std=c++17','-O1','-fno-exceptions','-fno-rtti','-I'+str(root/'src'),str(cpp),'-o',str(exe)]
 result=subprocess.run(cmd,text=True,capture_output=True)
 if result.returncode:raise RuntimeError(result.stderr[-3000:])
 subprocess.run([str(exe)],check=True)
