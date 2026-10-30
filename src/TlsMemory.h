#pragma once
#include <stdint.h>
#include <atomic>
// Shared TLS memory policy. Cloud.cpp installs the mbedTLS allocator hook and probes it;
// Live.cpp uses the same start-of-handshake gate so both TLS users agree.
namespace TlsMemory{
 inline std::atomic<bool>& external(){static std::atomic<bool> v{false};return v;}
 inline std::atomic<uint32_t>& hits(){static std::atomic<uint32_t> v{0};return v;}
 // Internal heap needed to START a handshake. With the PSRAM hook the 2x16 KB record buffers,
 // certificates and handshake state live in PSRAM, so only ~15-20 KB internal is used and the
 // rest is kept for lwIP/HTTP. Without the hook the old ~60 KB internal demand applies, so
 // defer rather than starve the local dashboard.
 inline bool enough(uint32_t freeHeap,uint32_t largest){return external().load()?(freeHeap>=44000&&largest>=14000):(freeHeap>=90000&&largest>=40000);}
}