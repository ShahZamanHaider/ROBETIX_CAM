#pragma once
#include <Arduino.h>
#include <atomic>
namespace Network { void begin(); void tick(); void changed(); extern std::atomic<uint32_t> reconnects; }
