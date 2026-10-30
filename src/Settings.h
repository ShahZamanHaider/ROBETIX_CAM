#pragma once
#include <Arduino.h>
#include <Preferences.h>
extern Preferences prefs;
String randomHex(size_t bytes);
String passwordHash(const String &password);
bool provisionFactoryLabel();
void settingsBegin();
bool factoryResetSettings(bool credentialsRevoked);
bool setAdminPassword(const String &password);
extern String adminHash, authSalt;
extern String setupSsid, setupPassword, hardwareTag;
extern bool cloudCleanupPending,factoryLabelNumeric;
