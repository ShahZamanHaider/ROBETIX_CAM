# Robetix Vision — standalone CCTV upgrade

Original AI Thinker ESP32-CAM, OV2640, microSD, and a phone/browser. Essential operation is local: no inference server, continuously running computer, cloud account, or external JavaScript/CDN. This is a compilable development release, **not a hardware-qualified finished product**. No connected serial device was available for this upgrade. The earlier camera/card failures cannot be diagnosed conclusively without boot logs and hardware checks.

## Build and flash

PlatformIO `espressif32@6.11.0`, Arduino-ESP32 2.0.17, ArduinoJson 6.21.5, bundled esp32-camera/SD_MMC/WiFi. No new firmware library dependencies. `huge_app.csv` gives a 3 MiB application partition; OTA is not implemented. `tools/embed_ui.py` embeds `data/index.html` into generated `src/Ui.h`; no filesystem upload is needed.

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -t upload --upload-port COMx
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor -b 115200
```

Replace COMx with the actual serial port. Hold GPIO0 low during reset for flashing if your adapter requires it; release GPIO0 and reset for normal operation. Disconnect power before touching camera ribbon or card. Use a stable regulated 5 V supply and short cable; retain brownout protection. `backups/phase1-source.zip` preserves the pre-upgrade sources.

## First use and Wi-Fi

Join **Robetix-Setup**, temporary Wi-Fi password **RobetixSetup**, then open **http://192.168.4.1**. Create your own administrator password (12–64 characters). Choose AP (camera hotspot) or STA (your 2.4 GHz router) in the web page and set your own hotspot name/password. STA keeps the configured hotspot available for recovery. `http://robetix.local` works where the phone/router supports mDNS; the IP address is the fallback. Administrator and Wi-Fi passwords are separate. Existing provisioned settings are preserved.

Remember this device is optional. Up to four random session tokens are accepted; only salted hashes are stored. Remembered browser cookies last one year and survive camera reboot; the device-side token remains valid until sign-out, slot replacement or factory reset, because there is no dependable offline calendar for expiry. Nonremembered sessions last at most 12 hours and do not survive reboot. Normal entry opens Home; incomplete initial provisioning opens Settings. HTTP is local and unencrypted. Do not expose camera ports to the Internet.

## Everyday operation

Home has full-frame live view, Record/Stop & Save, Take Photo, Saved Files, Motion Alerts, picture preset and flashlight. The same preview moves into Settings; there is no second camera stream. A frame older than 2.5 seconds is hidden rather than presented as live. The cached firmware frame must be younger than 1.5 seconds. Settings preview can collapse. Fit/Fill/original pixels and display-only zoom are available under Advanced.

Settings exposes camera name, orientation, picture quality, AP/STA setup and Privacy. Advanced groups camera controls, recording, motion area/sensitivity, schedules, storage, and diagnostics. Pause Camera finalizes recording, disables light, deinitializes the sensor and suspends motion; it persists across reboot. Camera Reset restores image defaults, retaining Wi-Fi and files. Factory Reset requires typed confirmation, finalizes the clip, clears NVS ownership/settings/sessions, and **does not format or erase SD files**.

Flashlight uses GPIO4, LEDC channel 6 / timer 3, distinct from camera XCLK timer 0. SD uses one-bit mode so GPIO4 is not an SD data signal. Output is provisionally capped at 25% with a 60-second timeout, default off. This cap is conservative, **not a measured thermal guarantee**. Do not increase it without electrical and temperature testing.

## Actual recording implementation

Native OV2640 JPEG frames are placed into a video-only MJPEG RIFF/AVI container (`MJPG`, `00dc` chunks); there is no audio and no RGB-to-JPEG conversion. One capture task owns camera access. The same frame can feed SD writing, a bounded preview copy, an optional photo, and a sampled motion analysis. Network clients never own the sensor framebuffer. Camera benchmarks are exclusive.

Default recording: VGA, JPEG quality 18, 5 FPS ceiling, 60-second clips. Configurable 1–10 FPS and 5–300 seconds; the hard bounds are 3,000 frames and 256 MiB per clip. Actual FPS is limited by capture, SD and motion costs. AVI playback rate derives from elapsed capture time; time-lapse instead plays at 5 FPS. Time-lapse interval is 1–60 seconds (0 disables it), and the clip must cover at least two intervals. It cannot be combined with Movement mode.

A 4 KiB staging buffer batches writes; data is flushed approximately once per second. Stop scans existing chunk headers in batches of 32 to construct `idx1`, patches RIFF/header lengths and timing, closes, then renames `.avi.part` to `.avi`. Failed writes stop recording and retain the unfinished file. There is no unbounded in-memory index. One recording session is allowed. Recording holds the SD operation lock; browsing, downloading and deletion report busy instead of competing with recording. Slow preview is expected under load.

Automatic modes are Off, Always Record, and Record on Movement. These settings persist; automatic operation retries when eligible and starts new bounded clips. Stop also turns automatic mode Off. Always mode has short gaps during finalization/restart and is **not gapless surveillance**. Automatic recording remains an experimental hardware-validation feature, off by default.

Unexpected power loss can leave the last `.part` incomplete, including its header/index and buffered tail. Boot counts these files and preserves them; automatic salvage is not implemented. Copy the card before attempting external recovery. Completed files remain separate. The firmware does not claim power-loss-proof FAT storage.

## Picture profiles and resource limits

| Profile | Native JPEG image | Quality number | Preview ceiling |
|---|---|---|---|
| Low | QVGA 320×240 | 24 | 3 FPS |
| Normal | VGA 640×480 | 18 | 5 FPS |
| Best | SVGA 800×600 | 14 | 3 FPS |
| Custom | Selected supported resolution, up to UXGA | 10–40 | Selected 1–10 FPS |

These are engineering starting values, not measured optimums. Lower JPEG numbers request higher quality/larger files. With no PSRAM, resolution is restricted to QVGA and preview buffers are smaller. One camera framebuffer is used. Preview plus the two web-server scratch buffers use up to 3×192 KiB PSRAM (3×32 KiB without PSRAM), excluding driver memory. Large JPEGs can exceed preview capacity and are skipped; lower resolution/quality to restore preview. Capture queues are bounded to four commands. Recording settings are rejected while recording/benchmarking; stop first.

Firmware verifies PID `OV2640_PID` before using native `PIXFORMAT_JPEG`. Boot probes conservatively and retries QVGA after an initialization failure. A sensor PID check is useful identification, not proof against counterfeit hardware. The actual connected sensor has not been observed here. SD mount tries one-bit 20/10/5 MHz and logs failures without formatting.

## Motion and alerts

This is **movement detection, not person detection or face recognition**. At most once per second, the ROM JPEG decoder emits 1/8-scale RGB blocks directly into a 32×24 luminance grid. There is no full-resolution RGB framebuffer. Comparison subtracts global brightness change, applies per-pixel thresholds, rejects isolated changes, confirms consecutive movement, and uses a cooldown. A normalized rectangle restricts the comparison region. Motion analysis is skipped above SVGA or when the previous SD frame write took at least 50 ms. Decoder callbacks abort after a provisional 100 ms budget; actual timing is reported, not guaranteed.

When nobody is viewing, motion monitoring temporarily uses QVGA; viewing restores the selected picture. Large scene changes, shadows, camera shake and poor lighting can still cause false or missed events. Recording may reduce sampling. Event photos and optional automatic clips share the capture task. The event ring stores at most 32 records and returns the latest 12; persistence is best-effort if SD is busy. Missing/removed media links may remain in historical events.

The Activity page provides in-app alerts and opt-in browser audio with Test, Stop, Mute and Snooze. Audio needs an explicit tap and browser permission. It does not deliver background push, SMS, email, closed-browser or locked-phone guarantees.

## Storage and clock safeguards

Only validated Robetix filenames in `/robetix/videos` and `/robetix/photos` are managed. Legacy `/snapshots` photos remain accessible. Traversal/arbitrary paths are rejected. Listings return at most 12 files and scan at most 4,096 directory entries. Larger libraries need a card reader; narrowing dates does not remove that scan cap. AVI downloads require a compatible player such as VLC; no misleading HTML video playback control is shown.

Individual and selected-page deletion require confirmation. Date search reports count, bytes, date bounds and a confirmation token; deletion is a separate authenticated request, bounded to the search's maximum sequence number so new files are excluded. Date endpoints are inclusive in the browser's timezone. Active `.part`, unrelated and undated files are excluded from date deletion. A failed delete stops the batch; the status reports partial progress. Four recent deletion summaries are held in RAM.

Optional free-space cleanup is OFF by default, deletes at most eight oldest completed Robetix videos before a new clip, never photos/unrelated/unfinished files, and reserves the configured free-space margin. Separate age retention for photos/videos is OFF by default; it waits for an idle SD card and a valid clock. A pending date-search confirmation pauses retention. Search/delete operations are serialized with other SD users.

The clock is synchronized by an authenticated browser or optional NTP when STA has Internet. Offline boot without synchronization has no invented date: filenames/events use epoch zero, schedules and age retention wait. Scheduling applies only to automatic recording, supports overnight windows and weekday groups, and uses a saved fixed UTC offset. Resave the schedule after daylight-saving changes. A trustworthy phone/router time is required; there is no battery-backed RTC.

## Testing and known limits

See `VALIDATION.md` for the actual software results and required device sequence. Compile success and simulated HTTP tests do not verify OV2640 initialization, valid driver JPEGs, card write latency, sustainable FPS, runtime heap, GPIO behavior, Wi-Fi range or overnight stability.

Deferred: automatic thermal/performance profile adaptation and web-server task restart; automatic interrupted-AVI repair; in-browser AVI playback; Urdu localization; HTTPS/full PWA/background notifications; OTA; gapless recording; person detection; face detection/recognition. No fake AI controls or CPU temperature readings are supplied. The firmware cannot prevent all overheating or compensate for a weak power supply.

Historical Arduino-ESP32 1.0.6 CameraWebServer used `fd_forward.h` and `fr_forward.h`: detection locates faces, recognition compares identities against enrollment. That dependency stack differs from this pinned Arduino 2.0.17 build; modern ESP-WHO support must not be assumed for original ESP32. Any future branch needs a verified compatible inference library, PSRAM/flash budget, JPEG decode and capture scheduling, measured latency/accuracy, enrollment access control, template deletion/privacy and spoofing assessment. This release is independent of AI.

References studied, not copied wholesale: [ESP32-CAM_MJPEG2SD](https://github.com/s60sc/ESP32-CAM_MJPEG2SD) (`src/avi.cpp`, `src/motionDetect.cpp`; its AGPL implementation and newer framework are not imported); [Microsoft AVI RIFF specification](https://learn.microsoft.com/en-us/windows/win32/directshow/avi-riff-file-reference); [Espressif camera driver](https://github.com/espressif/esp32-camera); [historical CameraWebServer](https://github.com/espressif/arduino-esp32/tree/1.0.6/libraries/ESP32/examples/Camera/CameraWebServer); [ESP-WHO](https://github.com/espressif/esp-who).

## Changed files

Updated: `platformio.ini`, `src/main.cpp`, `src/Settings.h/.cpp`, `src/Network.cpp`, `src/Camera.h/.cpp`, `src/Storage.h/.cpp`, `src/Web.cpp`, generated `src/Ui.h`, `README.md`, `VALIDATION.md`, `tools/test-ui.cjs`.

Added: `data/index.html`, `src/AppOptions.h`, `src/Engine.h/.cpp`, `src/AviWriter.h`, `src/Recording.h`, `src/MotionCore.h`, `src/Motion.h/.cpp`, `src/Events.h/.cpp`, `src/Auth.h/.cpp`, `src/Light.h/.cpp`, `src/Maintenance.h/.cpp`, `src/Schedule.h`, `tools/embed_ui.py`, `tools/test_avi.cpp`, `tools/test_avi.py`, `tools/test_core.cpp`. Host-only Playwright/Zig/PyAV/Pillow tooling lives under ignored `.pio`; none is required on the camera or a continuously running computer.
