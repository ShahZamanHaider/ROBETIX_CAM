#pragma once
#include <WiFiClientSecure.h>
// Preserve the stock verified handshake while retaining diagnostics before
// WiFiClientSecure::stop() frees the failed handshake's verification result.
class VerifiedTLS:public WiFiClientSecure {
public:
 uint32_t verification=0;int result=0;
 using WiFiClientSecure::connect;
 int connect(const char*host,uint16_t port) override {
  verification=0;IPAddress address;if(!WiFi.hostByName(host,address)){result=-1;return 0;}
  result=start_ssl_client(sslclient,address,port,host,_timeout,_CA_cert,_use_ca_bundle,_cert,_private_key,_pskIdent,_psKey,_use_insecure,_alpn_protos);
  _lastError=result;if(result<-1||result>=0)verification=mbedtls_ssl_get_verify_result(&sslclient->ssl_ctx);
  if(result<-1){const mbedtls_x509_crt*peer=mbedtls_ssl_get_peer_cert(&sslclient->ssl_ctx);
#if defined(MBEDTLS_SSL_KEEP_PEER_CERTIFICATE)
   // A failed handshake has no established session yet. Inspect only public
   // certificate subjects before stock stop() frees the negotiating session.
   if(!peer&&sslclient->ssl_ctx.session_negotiate)peer=sslclient->ssl_ctx.session_negotiate->peer_cert;
#endif
   for(int n=0;peer&&n<5;peer=peer->next,n++){char subject[160],issuer[160];mbedtls_x509_dn_gets(subject,sizeof(subject),&peer->subject);mbedtls_x509_dn_gets(issuer,sizeof(issuer),&peer->issuer);Serial.printf("ROBETIX_TLS_PEER: %s <- %s\n",subject,issuer);}}
  if(result<0||verification){stop();return 0;}_connected=true;return 1;
 }
};
