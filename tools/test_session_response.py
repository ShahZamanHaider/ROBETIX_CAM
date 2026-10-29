from pathlib import Path
import argparse,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--compiler',default='g++');p.add_argument('--prove-old',action='store_true');args=p.parse_args()
source=(root/'src/Web.cpp').read_text(encoding='utf-8')
def branch(s):
 a=s.index('  if(r->method==HTTP_POST&&uri=="/api/hotspot-session")')
 b=s.index('  if(r->method==HTTP_POST&&uri=="/api/accept-handoff")',a)
 return s[a:b]
prefix=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const char*headerValue=nullptr;static bool expired=false,expiredAtSend=false,headerError=false,cookieMemoryError=false,ap=true,origin=true,authenticated=false,issueFailure=false;static int issued=0,revoked=0,lastStatus=0;static bool sentCookie=false;
struct String{
 char*value;
 String(const char*s=""){value=(char*)malloc(strlen(s)+1);strcpy(value,s);}
 String(const String&s):String(s.value){}
 ~String(){if(headerValue==value)expired=true;free(value);}
 String&operator=(const String&s){if(this!=&s){if(headerValue==value)expired=true;free(value);value=(char*)malloc(strlen(s.value)+1);strcpy(value,s.value);}return *this;}
 size_t length()const{return strlen(value);}bool reserve(size_t){return !cookieMemoryError;}bool concat(const char*s){char*next=(char*)malloc(strlen(value)+strlen(s)+1);strcpy(next,value);strcat(next,s);free(value);value=next;return true;}bool isEmpty()const{return !*value;}const char*c_str()const{return value;}bool operator==(const char*s)const{return !strcmp(value,s);}
};
String operator+(const char*a,const String&b){char text[256];snprintf(text,sizeof(text),"%s%s",a,b.c_str());return String(text);}
String operator+(const String&a,const char*b){char text[256];snprintf(text,sizeof(text),"%s%s",a.c_str(),b);return String(text);}
namespace Auth{String issue(bool){issued++;return String(issueFailure?"":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");}void revoke(const String&){revoked++;}}
using esp_err_t=int;static const int ESP_OK=0,HTTP_POST=1;struct httpd_req_t{int method=HTTP_POST;};
template<int N>struct StaticJsonDocument{};template<class T>static bool body(httpd_req_t*,T&){return true;}
static bool cameraWifi(httpd_req_t*){return ap;}static bool sameCameraOrigin(httpd_req_t*){return origin;}static bool authorized(httpd_req_t*){return authenticated;}
static int httpd_resp_set_hdr(httpd_req_t*,const char*name,const char*value){assert(!strcmp(name,"Set-Cookie"));if(headerError)return -1;headerValue=value;return ESP_OK;}
static int reply(httpd_req_t*,const char*,const char*status="200 OK"){lastStatus=atoi(status);if(headerValue){if(expired){expiredAtSend=true;return -9;}assert(!strncmp(headerValue,"rv=",3));assert(strstr(headerValue,"HttpOnly"));assert(strstr(headerValue,"SameSite=Strict"));assert(strstr(headerValue,"Max-Age=31536000"));assert(strlen(headerValue)>64);sentCookie=true;}return ESP_OK;}
'''
suffix=r'''
return -8;}
static void reset(){headerValue=nullptr;expired=false;expiredAtSend=false;headerError=false;cookieMemoryError=false;ap=true;origin=true;authenticated=false;issueFailure=false;issued=revoked=lastStatus=0;sentCookie=false;}
int main(){httpd_req_t r;reset();int result=grant(&r);if(expiredAtSend){puts("REPRODUCED: response used a destroyed cookie owner");return 10;}assert(result==ESP_OK&&lastStatus==200&&sentCookie&&issued==1);
reset();authenticated=true;assert(grant(&r)==ESP_OK&&issued==0&&!sentCookie);reset();ap=false;grant(&r);assert(lastStatus==403&&issued==0);reset();origin=false;grant(&r);assert(lastStatus==403&&issued==0);reset();issueFailure=true;grant(&r);assert(lastStatus==503&&!sentCookie);reset();cookieMemoryError=true;grant(&r);assert(lastStatus==503&&revoked==1&&!sentCookie);reset();headerError=true;grant(&r);assert(lastStatus==503&&revoked==1&&!sentCookie);puts("PASS: actual hotspot handler keeps borrowed cookie alive through send, respects access checks, and fails closed on storage/header errors.");}
'''
with tempfile.TemporaryDirectory(prefix='rv-session-')as temporary:
 folder=Path(temporary);cpp=folder/'session.cpp';exe=folder/('session.exe'if __import__('os').name=='nt'else'session')
 def run(s):
  helper=s[s.index('static esp_err_t sendLocalSession('):s.index('static bool authorized(')]if 'static esp_err_t sendLocalSession('in s else ''
  cpp.write_text(prefix+helper+'static int grant(httpd_req_t*r){String uri("/api/hotspot-session");\n'+branch(s)+suffix,encoding='utf-8');cmd=[args.compiler]
  if 'zig'in Path(args.compiler).name:cmd+=['c++','-nostdlib++']
  subprocess.run(cmd+['-std=c++17','-O1','-fno-exceptions','-fno-rtti',str(cpp),'-o',str(exe)],check=True);return subprocess.run([str(exe)]).returncode
 if args.prove_old:
  old=subprocess.check_output(['git','show','3b04f7a:src/Web.cpp'],cwd=root,text=True);assert run(old)==10,'Old handler must reproduce lifetime failure'
 assert run(source)==0
