#include "Settings.h"
#include <esp_system.h>
#include <mbedtls/sha256.h>
Preferences prefs;
String adminHash, authSalt;
String randomHex(size_t bytes) {
  String out; out.reserve(bytes * 2);
  for(size_t i=0;i<bytes;i++) { char b[3]; snprintf(b,sizeof(b),"%02x",uint8_t(esp_random())); out += b; }
  return out;
}
String passwordHash(const String &password) {
  String input=authSalt+password; uint8_t digest[32];
  mbedtls_sha256_ret((const unsigned char*)input.c_str(),input.length(),digest,0);
  String out; for(auto b:digest) { char h[3]; snprintf(h,3,"%02x",b); out+=h; } return out;
}
void settingsBegin() {
  prefs.begin("robetix",false);
  adminHash=prefs.getString("admin","");
  // Preserve configured installations; unfinished old setups use the documented
  // bootstrap AP so setup never depends on reading a random serial password.
  if(!prefs.isKey("netReady")) prefs.putBool("netReady",!adminHash.isEmpty());
  authSalt=prefs.getString("salt","");
  if(authSalt.isEmpty()) { authSalt=randomHex(16); prefs.putString("salt",authSalt); }
}

