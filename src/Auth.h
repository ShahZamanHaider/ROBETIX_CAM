#pragma once
#include <Arduino.h>
namespace Auth {void begin();String issue(bool remember);bool valid(const String&token);void revoke(const String&token);String armRecovery();String trust(const String&,String&);String handoff(const String&);String acceptHandoff(const String&,bool&);bool authorize(const String&,const String&,String&);bool sensitive(const String&);bool recover(const String&,const String&,String&);}
