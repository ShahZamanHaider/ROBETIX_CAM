#pragma once
#include <Arduino.h>
namespace Auth {void begin();String issue(bool remember);bool valid(const String&token);void revoke(const String&token);}
