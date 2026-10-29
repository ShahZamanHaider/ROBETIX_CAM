# Remote CCTV v2 — development checkpoint

The production release is preserved by `production-2026-10-29` and
`checkpoint/pre-full-remote-cctv`. This upgrade belongs on
`feat/full-remote-cctv`; production promotion is a separate approval.
No database migration, user reset, camera removal, SD format or NVS erase is
required. Existing cameras require a firmware update to advertise control
protocol 2. Old firmware retains its existing viewer compatibility.

## Streaming correction

The previous camera transport closed its WebSocket before HTTPS cloud requests,
then could reconnect using an already consumed, single-use relay ticket.
Transient disconnects had the same problem. The new transport invalidates the
ticket whenever the connection closes and requests a fresh proof with bounded
retry. This is a source-level defect; identifying the cause on a particular
physical board still needs its serial/status diagnostics.

Ticket expiry uses a monotonic lease calibrated to the service response. Frame
timestamps use relay/server time and the capture's monotonic timestamp. An
incorrect camera RTC no longer determines frame freshness or ticket validity.
TLS still verifies the CA bundle; initial clock synchronization remains needed
for cloud HTTPS. Relay frame acknowledgements, handshake code, rejected-frame
reason, heap, PSRAM and capture/storage diagnostics remain available to owners.

The viewer removes stale images, acknowledges only decoded JPEGs, requests
fresh credentials on reconnect, and offers Retry. Camera heartbeat availability
and receiving video are separate states. Privacy mode leaves the command
connection available so an owner can resume the camera.

## Actual remote commands

`POST /api/remote` requires the existing owner session, CSRF/origin checks,
camera ownership, enabled remote access, a recent heartbeat and protocol 2.
Commands carry 128-bit random IDs. A 90-second HMAC ticket binds the device and
exact request-body hash. The relay forwards the command over the camera's
outbound authenticated WSS connection. It returns success only after the camera
reports completion. No port forwarding or public camera HTTP endpoint is used.

Implemented operations: current status, record, stop/finalize, card photo,
flashlight level/preferences, picture controls, motion/recording options,
privacy, restart, card listing, file information/chunks, individual deletion.
Restart additionally requires a recent sign-in. Picture/configuration changes
are rejected while recording by the existing capture service.

The existing Engine queue owns capture and recording. Recording acknowledgement
waits for its first successful AVI frame; a photo acknowledgement waits for its
card save. The relay persists up to 64 recent mutation receipts for five minutes
and reserves IDs atomically. An unconfirmed command is not automatically sent
again. Check current status after a timeout. Camera reboot/relay failures can
leave a result unknown; this does not provide distributed exactly-once execution.

## Card files

Files stay on the ESP32 microSD. Listings have 12 entries and retain the existing
4096-entry scan bound. Only validated Robetix filenames and folders are accepted.
Downloads use scoped, origin-bound 90-second file tickets, renewed through the
owner's authenticated session. Each SD read locks, seeks, reads at most 8192
bytes (1024 without PSRAM), closes and releases the card before network output.
Saved-file operations reject while recording owns the card. No AVI is loaded
whole into firmware RAM, Vercel RAM or browser application RAM.

A same-origin service worker streams chunks into the browser download manager;
it caches neither files nor credentials. Pending and active downloads are capped
at two and cancelled on sign-out/camera change. Tickets exist in RAM only.
Browser download support is required. AVI playback is by download to a compatible
player. Photo preview is capped at 2 MiB. No public media URLs are created.

The memory-card diagnostics report 1-bit mode, attempted frequency, mount result,
card type and file errors. Failure cannot distinguish a missing, defective,
incorrectly formatted or underpowered card without physical checks. Power off
before handling the card; no formatting is attempted automatically.

## Interface and limits

Home, Saved Files, Activity and Settings share branding, color tokens, icons and
the continuous browser alarm engine with the local app. English/Urdu uses the
existing dictionary and RTL renderer. The cloud viewer offers fullscreen,
pause/resume, quality and digital zoom; card photos are separate from saving a
currently received viewer frame. Camera selection scopes every command.
WhatsApp recipient management is retained; provider configuration is not a
claim of delivered messages. Motion remains local motion detection, not AI.

Remote live ceilings remain conservative: 1 FPS Low, up to 2 FPS Normal/Best.
They are configured ceilings, not measured hardware performance. Native JPEG
uses one bounded PSRAM slot; recording/capture remain in the existing loop.
Extra command JSON trees prefer PSRAM. Physical FPS, SD latency, simultaneous
local/remote viewing, heat/power and long-duration heap stability require board
testing. Mobile OS background audio restrictions still apply.

## Validation / physical sequence

Run backend/relay tests, browser controls/download tests, the host LiveLease
test, translation/secret scans and PlatformIO builds before release. Browser and
cloud synthetic-camera tests prove software paths, not physical hardware.

1. Connect the ESP32-CAM-MB by a data USB cable. First read existing diagnostics;
   do not erase settings or re-pair an already owned camera unnecessarily.
2. After Preview is verified, use `esp32cam_preview` with ignored, deployment-pinned
   `.pio/preview-access.json` for authorized hardware development. Never place
   a bypass credential in firmware. The isolated Preview relay has its own secret.
3. Verify repeated fresh frames, reconnect after Wi-Fi loss, local viewing,
   flashlight, photo, record/stop, playable AVI download, delete confirmation,
   motion events, privacy resume, owner isolation and offline local operation.
4. Record heap/PSRAM and SD write diagnostics during a sustained run. Correct
   physical SD/power/radio failures before claiming acceptance.
5. Only after approval deploy/promote the tested cloud and relay together, then
   build/upload `esp32cam` for the permanent production origin
   `https://vision.robetix.com`. Do not flash a development origin as production.

USB command `DIAGNOSTICS` prints the same bounded, read-only status used by the
authenticated dashboard. It contains no passwords, session cookies or device
credentials. Existing `TRUST_DEVICE` recovery remains available.

Physical checkpoint (2026-10-29): the connected original ESP32-CAM identified
OV2640/native JPEG and about 4 MiB PSRAM. The Preview firmware uploaded and its
flash hash verified without erasing NVS. SD initialization timed out (0x107) at
20/10/5 MHz. The saved router was unavailable (Wi-Fi reason 201), so the camera
had no router IP, no cloud handshake and zero remote frames. Home Wi-Fi setup
and card inspection remain necessary; recording and physical remote viewing
have not passed acceptance. Repeated synthetic cloud tests also encountered
intermittent browser WebSocket handshake failures; a focused deployed test
passed initial video, fresh-ticket producer reconnection and repeated Retry.
Do not treat that as a sustained physical-device stability test.
