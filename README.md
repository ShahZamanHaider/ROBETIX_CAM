# Robetix Vision

Standalone CCTV firmware for the original AI Thinker ESP32-CAM, native-JPEG camera and microSD. The camera serves its own phone-friendly application. Essential local operation needs no cloud service or continuously running computer.

This is a software-tested development release. No ESP32 serial device was available during this upgrade; camera/card behavior, detection accuracy, sustained FPS, electrical limits and overnight stability have **not** been verified on hardware. See [VALIDATION.md](VALIDATION.md) for the actual evidence and test sequence.

## Build, flash, connect

Pinned environment: PlatformIO Espressif32 **6.11.0**, Arduino-ESP32 **2.0.17**, ArduinoJson **6.21.5**. HTTPClient and WiFiClientSecure are bundled framework libraries. The huge_app partition provides 3 MiB application space; OTA is not implemented. HTML, CSS, alarm JavaScript and footer are embedded into flash by `tools/embed_ui.py`; no filesystem upload is required.

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -t upload --upload-port COMx
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor -b 115200
```

Replace COMx with the actual serial port. If your adapter requires it, hold GPIO0 low during upload reset; release it and reset for normal use. Power off before reseating a camera ribbon or card. Use a stable regulated 5 V supply and short cable. Brownout protection remains enabled.

First use: join **Robetix-Setup**, password **RobetixSetup**, open **http://192.168.4.1**, create your own 12–64-character administrator password, then select AP or STA and set your Wi-Fi credentials through the web page. STA joins a 2.4 GHz router while retaining the configured recovery hotspot. `http://robetix.local` works where mDNS is supported; use the actual IP otherwise. Existing provisioning is retained. Administrator and hotspot passwords are separate.

## Current functionality

- Home: full-frame preview, Record/Stop & Save, Take Photo, Saved Files, movement status, flashlight and display zoom. Desktop has a compact side rail; phones have bottom navigation. The header uses one camera identity. The requested links appear in the compact public footer.
- Home and Settings share one preview. Fit, zoom/reset, fullscreen and two-finger zoom operate in the browser. They do not create optical zoom or increase detection detail. Old frames hide after 2.5 seconds; firmware rejects cached frames older than 1.5 seconds. Offline, paused, saving, low-space and missing-card states remain explicit.
- Native MJPEG AVI recording; persistent Off/Always/Movement modes; bounded clips; snapshots; SD photo/video listing, downloads and confirmed deletion; date search/summary/confirmation; optional oldest-video space cleanup and age retention; schedules and time-lapse; privacy; network recovery; camera reset, restart and factory reset; system diagnostics.
- Persistent browser alarm with prominent movement banner and Stop Alarm. Enable sound with a tap; default mode rings until acknowledgement. Volume, three original synthesized tones, Test, Mute, 5/15/30-minute Snooze, and an explicit timed mode are available. One continuously looping Web Audio buffer produces the sound without timer-driven beep replenishment. Stop releases the source, gain and housekeeping timer; the unlocked AudioContext is reused. New distinct events can rearm it. Duplicate events update history without overlapping sound. UI playback status follows the audio context, including suspension and zero volume. Muting does not stop firmware history or recording. An unacknowledged alert is retained in this browser across reloads, scoped to the camera’s public identifier; sound still needs a new unlock tap after reload. Tone/volume/mode preferences are browser-local. Closed pages, sleeping phones and suspended browser audio cannot provide guaranteed alarms.
- Simple Motion Detection controls, isolated Test Motion Detection and optional movement overlay, described below. Detailed tuning remains under Advanced Settings.
- English and Pakistani Urdu throughout the local and optional remote interfaces, including dynamic errors and confirmation dialogs. Settings switches language immediately; browser-local preference survives reload, without restarting the camera. Urdu uses RTL and system fonts; filenames, camera names, network names, URLs and machine values retain their original content. No translation framework or font CDN is required.
- Optional verified-HTTPS cloud agent and a separate Vercel/Supabase remote application in `remote/`. Includes device pairing, private motion photos/activity, account authorization, revocation and official Meta template/media request code. These require your accounts, deployment, provider approval and end-to-end tests. They are **Off by default**. No cloud service, DNS, GitHub remote or Meta account was provisioned automatically.
- Dynamic WhatsApp recipient management is in the authenticated hosted dashboard, linked from the camera's existing WhatsApp section: add/edit/enable/disable/remove up to five contacts, explicit consent, individual movement/photo/interval preferences, one-contact tests and server pause. Numbers are encrypted on the backend and not returned to the camera. See [remote/RECIPIENTS.md](remote/RECIPIENTS.md) for migration and setup; no real delivery/deployment is claimed.

## Sensors and camera controls

The installed Espressif driver probes and identifies the sensor. The operational native-JPEG set for this release is **OV2640, OV3660 and OV5640**. Pin mapping remains AI Thinker. Physical connector, pinout, supply and lens compatibility still require checking; none of these modules was physically tested here. OV2640 remains the preferred baseline. The firmware reports model/PID, native-JPEG capability, bounded resolution choices and image controls. Unknown/incompatible modules show an explicit error. A small RGB565 probe may identify a non-JPEG sensor after an unsupported-format initialization error; it is not used for runtime streaming or software JPEG conversion. Uncompressed/software-JPEG operation is deferred because its performance/memory cost has not been verified.

Resolution is restricted to QVGA without PSRAM. Supported advanced choices are bounded to UXGA even when the sensor can output more. The benchmark tries QVGA through UXGA at JPEG qualities 12/20, 30 measured frames per combination, with capture errors, heap and PSRAM. It owns capture exclusively and pauses normal work. The user must save results; no benchmark results are fabricated. Image changes are deferred/rejected while recording or benchmarking.

| Picture profile | Native size | JPEG quality | Preview ceiling |
|---|---|---|---|
| Low | QVGA 320×240 | 24 | 3 FPS |
| Normal | VGA 640×480 | 18 | 5 FPS |
| Best | SVGA 800×600 | 14 | 3 FPS |
| Custom | Supported selected size | 10–40 | Selected 1–10 FPS |

These are conservative engineering starting points, not measured optimums. Lower JPEG numbers request larger/clearer images. Best is not a guarantee of simultaneously maximal recording/motion performance. Size changes reinitialize buffers, and controls/settings are restored. Mirror, flip, brightness, contrast, saturation, white balance, exposure and gain are available for the accepted sensor families.

## Motion detection

The old 32×24 whole-zone comparison was replaced by a **64×48 luminance grid**. The ROM JPEG decoder emits scaled blocks (1/4 for QVGA, 1/8 for VGA/SVGA) directly into grid accumulators; no full RGB image is allocated. Grid data and the background model live in a bounded PSRAM workspace. If PSRAM/workspace is unavailable, motion is reported unavailable while the local camera remains usable.

Motion v2 combines frame and adaptive background differences, global brightness normalization, a per-cell noise estimate, connected-neighbor filtering, and local 4×4-cell regions (192 regions). Local scoring can confirm a small clustered change without requiring a large percentage of the full frame to change. Detect Smaller Movements relaxes connected-cell requirements and considers clusters spanning region boundaries. Strong global light changes and a coarse one-cell camera translation are suppressed and relearned; this is a limited heuristic, not image stabilization.

The lifecycle is Monitoring → Possible → Confirmed → Event Active → Motion Ended → Ready → Monitoring, with a separate minimum interval between event starts. Settled recognized foreground is tracked so it does not remain a new event forever; returning to the reference scene clears occupancy and permits a new entry. Roughly 750 ms of actual frame stability ends an event. Confirmation defaults to one second and the start interval to two seconds. This replaces the old requirement for 30 uninterrupted quiet seconds, which could prevent repeated hand movements from rearming. Existing configurations lacking the motion_v2 marker are migrated once to these two timing defaults; other settings are retained. Custom timing remains under diagnostics.

Low/Normal/High change thresholds and local-cell requirements. Custom exposes threshold, connected neighbors and minimum local cells under diagnostics. Sampling interval is 250–2000 ms (default 500 ms); actual sampling can be slower than the ceiling under load. No viewer temporarily uses QVGA; viewing restores the selected profile. Analysis above SVGA is skipped. Analysis is skipped when the previous SD frame write took at least 50 ms; decoder callbacks abort after a provisional 100 ms budget. Separate algorithm_ms and decode_ms diagnostics expose real device costs. Capture/SD latency and actual processing time must be measured on hardware; host tests are not ESP32 performance measurements.

Test Motion Detection uses a separate, lazily allocated PSRAM detector with the same sampled decode, showing Movement Detected, Scene normal or an adjustment status. Test samples never create motion events, automatic motion recordings, snapshots, cloud notifications or alarms, even when ordinary monitoring is enabled. Manual recording and explicitly selected Always recording remain independent. Stopping the test releases its detector and rebases normal monitoring. Show Movement Areas is a separate passive visualization subscription. The 192 coarse regions are transmitted only during either subscription, as a bounded hex string. Motion storage is fixed-size; the extra test detector is released afterward. No extra camera capture loop is introduced.

The motion event record includes a reserved persistent ID, camera name, reliable timestamp or zero, score, photo/video references and current notification state. Thirty-two records are bounded in RAM/SD; old history is migrated without deleting the original file. Provider state updates are best-effort RAM metadata until another event persistence operation/restart; the cloud database retains authoritative delivery receipts.

Synthetic tests confirm small clustered changes and reject an isolated changing cell. They do **not** establish detection distance or real-scene accuracy. Small subjects, shadows, exposure changes, wind, reflections, camera movement and sparse temporal sampling can still produce misses/false alerts. This detects movement; there is no person/face model. Digital zoom cannot recover details the lens/sensor did not capture.

## Flashlight and electrical limits

GPIO4 uses LEDC channel 6/timer 3; camera XCLK uses timer 0/channel 0. SD stays in 1-bit mode, freeing GPIO4. The Home brightness slider covers **0–100% of the supported range**, with On/Off, Remember brightness, and a selectable 10-second to 5-minute auto-off under Advanced. Preferred brightness/timer persist; light starts Off and turns Off on privacy/reset. No motion event automatically turns it on.

**The supported range is still capped at 25% electrical PWM duty.** Thus displayed 100% currently means 25% electrical duty. This is disclosed in the interface, not called full hardware power. There are no thermal measurements, current measurements or trustworthy on-chip temperature sensor readings available. Continuous full-power operation is not validated. A longer selected timeout is a bounded operator choice, not proof of safety; supervise initial testing and reduce brightness/time if the board or supply is unstable. Hardware validation is required before raising the compile-time duty cap or permitting indefinite operation.

## Recording, storage and clock

A single capture task coordinates native JPEG frames for preview, AVI, photos, motion and optional cloud copies. The camera framebuffer is returned promptly; networking consumes independent bounded copies. The command queue has four entries, cloud has one photo slot, and only one recording can own SD. Browsing/download/deletion reports busy while recording rather than competing with SD writes.

Recording is video-only `MJPG` RIFF/AVI, native `00dc` JPEG chunks, no audio. Defaults are VGA/quality 18, 5 FPS ceiling and 60 seconds. Limits: 1–10 FPS, 5–300-second clips, 3,000 frames and 256 MiB. A 4 KiB write buffer flushes roughly each second. Stop rebuilds `idx1` in 32-entry batches by scanning chunk headers, patches lengths/timing, closes and renames `.avi.part` to `.avi`. No unbounded RAM index is used. Actual FPS is bounded by capture/SD/motion. Always recording has finalization/restart gaps. Time-lapse plays at 5 FPS; interval 1–60 seconds and clip at least twice the interval. Movement/time-lapse cannot be combined. Stop also turns automatic recording Off.

Power interruption may leave the last `.part` incomplete, including buffered data/header/index. The firmware counts and preserves unfinished files; automatic repair is not implemented. Completed files remain separate. Download AVI to a compatible player such as VLC; browser AVI playback is not presented as working.

Only validated names in `/robetix/videos` and `/robetix/photos` are managed. Legacy `/snapshots` remains accessible. Listings are limited to 12 files and 4,096 scanned directory entries. Traversal/arbitrary paths are rejected. Date deletion first reports count, bytes and date range, then requires a separate token/confirmation; new sequence numbers and active/unfinished/undated/unrelated files are excluded. Delete failure stops the batch and reports partial progress. A pending search confirmation pauses age retention.

Optional space cleanup is Off by default: at most eight oldest completed Robetix videos before a new clip, preserving the configured free margin. Separate photo/video age retention is Off by default, waits for a valid clock and idle SD, and preserves undated files. SD mount retries one-bit 20/10/5 MHz without formatting. The no-card camera remains usable.

Browser time or optional NTP supplies the clock. Offline boot without sync uses unknown/zero time. Schedules and age retention wait for sync. Automatic schedules support overnight windows/weekday groups and a saved fixed UTC offset; resave after daylight-saving changes. Manual recording is not schedule-gated. There is no battery-backed RTC.

## Access and cloud setup

Remembered local sessions store hashes of random device tokens, not the administrator password in browser storage. Up to four slots; remembered cookie one-year browser lifetime and device validity until sign-out/replacement/factory reset. Nonremembered sessions last at most 12 hours and end on device reboot. Local HTTP is unencrypted and must not be port-forwarded publicly. Media/admin routes remain authenticated; mutations require JSON.

[remote/README.md](remote/README.md) explains what Supabase is, account creation, a new Vercel project, exact deployment variables, service-root certificate provisioning, pairing, Meta templates/webhooks, costs and `vision.robetix.com` DNS. Do not modify the main site/portfolio. The cloud client verifies HTTPS using the supplied trusted root and requires a valid clock. It never stores Meta credentials, uses bounded uploads/timeouts/retries, skips on low internal heap and does not block capture with network requests. Internet outage only affects optional features.

No actual cloud/account deployment, SQL execution, WhatsApp delivery or worldwide access was verified. Remote live video, remote camera commands, MFA UI and production security/availability certification are deferred. Browser alarms cannot force a locked phone or a WhatsApp notification to ring continuously.

## Evidence, preservation and remaining work

Baseline audit: [docs/AUDIT.md](docs/AUDIT.md). Baseline Git commit: `55fdd71`. Earlier source backup: `backups/phase1-source.zip`. The alarm/motion/language upgrade starts from checkpoint tag `checkpoint/pre-motion-v2` (`c306a41`). Its root causes, software evidence and physical acceptance sequence are in [docs/MOTION_V2_VALIDATION.md](docs/MOTION_V2_VALIDATION.md); earlier acceptance evidence remains in [VALIDATION.md](VALIDATION.md). The repository records the real changes; firmware and optional server are in this same project.

Deferred: measured automatic performance/thermal adaptation, interrupted AVI repair, AVI browser player, HTTPS/PWA installation on the ESP32, OTA, uncompressed sensor software JPEG operation, remote live relay/control, MFA UI, full production security audit, and face/person recognition. Historical Arduino-ESP32 1.0.6 face detection located faces; recognition compared enrolled identities. That old dependency stack differs from this pinned framework, and modern ESP-WHO support for the original ESP32 cannot be assumed. A future isolated AI branch needs verified compatible dependencies, RAM/flash/capture budgets, measured latency/accuracy and enrollment/template privacy/security. No such model is integrated here.

References: [Espressif camera driver](https://github.com/espressif/esp32-camera), [Microsoft AVI RIFF reference](https://learn.microsoft.com/en-us/windows/win32/directshow/avi-riff-file-reference), [ESP32-CAM_MJPEG2SD](https://github.com/s60sc/ESP32-CAM_MJPEG2SD) (techniques studied; AGPL implementation not imported wholesale), [ESP-WHO](https://github.com/espressif/esp-who). Current cloud references are linked in its setup guide.
