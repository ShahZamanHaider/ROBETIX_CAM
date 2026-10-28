#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../src/AviWriter.h"
#include <stdio.h>

#include <assert.h>
#include <stdlib.h>
struct Disk {
 FILE*f=nullptr;size_t failAfter=SIZE_MAX,written=0;
 size_t write(const uint8_t*p,size_t n){size_t allowed=n;if(written+n>failAfter)allowed=failAfter>written?failAfter-written:0;size_t r=fwrite(p,1,allowed,f);written+=r;return r;}
 size_t read(uint8_t*p,size_t n){return fread(p,1,n,f);}
 bool seek(uint32_t p){return !fseek(f,p,SEEK_SET);}bool flush(){return !fflush(f);}void yield(){}
};
int main(int argc,char**argv){
 assert(argc==4);FILE*jpeg=fopen(argv[1],"rb");assert(jpeg);fseek(jpeg,0,SEEK_END);long len=ftell(jpeg);rewind(jpeg);uint8_t*bytes=(uint8_t*)malloc(len);assert(bytes);assert(fread(bytes,1,len,jpeg)==size_t(len));fclose(jpeg);
 Disk io;io.f=fopen(argv[2],"w+b");assert(io.f);Avi::Writer<Disk>w(io);assert(w.begin(320,240));
 for(int i=0;i<15;i++)assert(w.append(bytes,len));assert(w.finish(3000));fclose(io.f);
 Disk bad;bad.f=fopen(argv[3],"w+b");assert(bad.f);bad.failAfter=Avi::HEADER_SIZE+12;Avi::Writer<Disk>failed(bad);assert(failed.begin(320,240));assert(!failed.append(bytes,len));assert(!failed.finish(1000));fclose(bad.f);
 puts("PASS: same C++ AVI writer finalized 15 frames; injected short write was rejected.");
}
