#pragma once
#include <Arduino.h>
#include <Preferences.h>
extern Preferences prefs;
String randomHex(size_t bytes);
String passwordHash(const String &password);
void settingsBegin();
bool setAdminPassword(const String &password);
extern String adminHash, authSalt;
extern String setupSsid, setupPassword, hardwareTag;
