# Public TLS roots

`mozilla.pem` is the Mozilla CA export from https://curl.se/ca/cacert.pem downloaded 2026-10-28, verified against curl's SHA256 file (`mozilla.sha256`). It contains public certificates, no private keys. Attribution/license: https://curl.se/docs/caextract.html (Mozilla Public License 2.0).

`tools/vendor/gen_crt_bundle.py` is Espressif's Apache-2.0 generator from ESP-IDF v4.4.7: https://github.com/espressif/esp-idf/blob/v4.4.7/components/mbedtls/esp_crt_bundle/gen_crt_bundle.py. Its original license header is retained. Generate `x509_crt_bundle` with Python + cryptography, then convert its bytes into `src/TrustBundle.h`. The firmware uses Arduino ESP32's verified `setCACertBundle`, with correct wall-clock time required. Legacy advanced HTTPS PEM override remains available. No insecure TLS fallback exists.

Refresh the pinned public trust store periodically and rebuild firmware. A TLS handshake failure must remain an error; never replace verification with `setInsecure()`.
