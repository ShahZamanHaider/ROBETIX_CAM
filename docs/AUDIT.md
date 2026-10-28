# Audit before the professional upgrade

Baseline is preserved as Git commit `55fdd71` and the earlier Phase 1 ZIP under `backups/`. The workspace had no Git repository; this upgrade initialized one before changing source. No upstream GitHub repository or Vercel project was created.

## Observed implementation

- `platformio.ini`: Espressif32 6.11.0, board esp32cam, Arduino framework package 3.20017.241212 (Arduino-ESP32 2.0.17), ArduinoJson 6.21.5, huge_app partition. HTML is embedded into PROGMEM by `tools/embed_ui.py`.
- `Camera.cpp`: OV2640 PID-only acceptance; native JPEG; one driver framebuffer; conservative initialization with QVGA retry; camera mutex. Size changes reinitialize the driver. Failure messages are exposed.
- `Engine.cpp`: a single capture task coordinates preview, photos, AVI, and motion. Four-command queue. Preview cache, HTTP scratch copies and recording buffers are bounded. Persistent Always/Movement modes, privacy, schedules/time-lapse and reset handling exist in code.
- `AviWriter.h`, `Recording.h`: actual MJPEG AVI with bounded SD buffering, final header/index scan and rename from .part. Interrupted .part repair was not implemented. SD is one-bit; file access is strictly validated and serialized with recording.
- `MotionCore.h`: 32×24 luminance grid, frame difference, whole-zone percentage requirement, neighbor filtering, consecutive confirmation and cooldown. One sample/second. Coarse spatial reduction plus percentage scoring can miss a small subject. No background reference or regional scoring existed.
- Browser audio: three short oscillator bursts. No repeating scheduler or persistent acknowledged alarm existed. This explains the reported short alarm directly from code.
- Light: GPIO4, LEDC channel 6/timer 3, independent of camera timer 0. Fixed 25% duty cap and 60-second timeout. No hardware safety measurement existed to justify raising electrical output.
- Wi-Fi: web AP/STA provisioning, recovery hotspot, mDNS, controlled retry; optional NTP. Administrator hashing/session controls, JSON-only mutations and protected media routes existed. No cloud or WhatsApp integration existed.

## Baseline evidence

`platformio run` succeeded: static RAM 72,712 / 327,680 bytes (22.2%), flash 1,112,149 / 3,145,728 bytes (35.4%). Existing simulated browser tests and portable AVI structure/JPEG payload checks were available. These do not establish physical camera/card functionality.

No serial device was detected in the available environment. There are no hardware boot logs establishing the cause of the reported camera/card failures. The code retains explicit faults and SD mount retries; no UI change is treated as a hardware repair. No FPS, detection-distance, thermal or overnight stability claims can be made.

## Design decision

The local camera experience is the primary product: a compact bracket/lens mark with one camera name, navy image surround, slate surfaces and restrained blue controls. The preview dominates; desktop navigation occupies a narrow left rail and phones keep a bottom bar. Technical information stays in Advanced Settings. Native system typography avoids network fonts and heavy bundles. Cloud features are opt-in and show their actual setup state.
