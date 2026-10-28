#pragma once
#include <Arduino.h>
namespace Auth {void begin();String issue(bool remember);bool valid(const String&token);void revoke(const String&token);String armRecovery();bool recover(const String&,const String&,String&);}
