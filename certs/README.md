# Public TLS roots

`mozilla.pem` is the Mozilla CA export from https://curl.se/ca/cacert.pem downloaded 2026-10-28, verified against curl's SHA256 file (`mozilla.sha256`). It contains public certificates, no private keys. Attribution/license: https://curl.se/docs/caextract.html (Mozilla Public License 2.0).

`tools/vendor/gen_crt_bundle.py` is Espressif's Apache-2.0 generator from ESP-IDF v4.4.7: https://github.com/espressif/esp-idf/blob/v4.4.7/components/mbedtls/esp_crt_bundle/gen_crt_bundle.py. Its original license header is retained. Generate `x509_crt_bundle` with Python + cryptography, then convert its bytes into `src/TrustBundle.h`. The original bundle is retained as reference; the current development firmware uses the selected native PEM anchors described below, with correct wall-clock time required. Legacy advanced HTTPS PEM override remains available. No insecure TLS fallback exists.

Refresh the pinned public trust store periodically and rebuild firmware. A TLS handshake failure must remain an error; never replace verification with `setInsecure()`.

The Arduino 2.x compact-bundle callback failed on the physical board
(`X509_FATAL_ERROR`, -12288). Desktop inspection received a cross-signed Vercel
chain. Native PEM still returned NOT_TRUSTED (flag 8) on the board; the precise
peer-chain cause is not yet established. This change is not a verified TLS fix. The five relevant
Mozilla anchors are now in `service-roots.pem`, reproduced with
`python tools/build-service-roots.py`; `--check` validates fingerprints and the
generated `src/TlsRoots.h`. Cloud HTTPS and relay WSS use native mbedTLS PEM
verification (`setCACert`) with these CA roots. These are public trust anchors,
not leaf certificates or private keys. Certificate signatures, hostname and
validity dates remain checked. The existing owner-supplied PEM override remains.
The old full binary bundle is retained as source/reference, not the default
runtime verifier. Refresh this selected store when service CA families change.

See [Espressif's cross-signed chain support documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_crt_bundle.html#cross-signed-certificate-support)
and the [Arduino bundle/native-PEM verification report](https://github.com/espressif/arduino-esp32/issues/8626).
