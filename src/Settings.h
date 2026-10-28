#pragma once
#include <Arduino.h>
#include <Preferences.h>
extern Preferences prefs;
String randomHex(size_t bytes);
String passwordHash(const String &password);
void settingsBegin();
extern String adminHash, authSalt;
constexpr const char *SETUP_AP_SSID = "Robetix-Setup";
constexpr const char *SETUP_AP_PASSWORD = "RobetixSetup";

