# Unified Robetix Vision application — 2026-10-30

This is software verification, not physical-device acceptance or a market release.
The ESP32-CAM was not connected during this work. Production and database records
were inspected read-only. No customer camera was removed, re-enabled or reset.

## Actual connectivity finding

The configured database returned one camera registration with `enabled=false`,
remote access selected, and last heartbeat `2026-10-28T21:06:03.277+00:00`.
The existing list deliberately labels this state Disconnected. Device authentication
requires both the matching credential hash and enabled=true, so reconnecting cannot
restore this registration. The record does not identify who revoked it. Do not
simply flip enabled or fabricate a fresh heartbeat. A physical, authorized pairing
must issue a new credential.

The permanent HTTPS domain and public health route returned 200. Production reports
commit bb4c2e158ffbf608893ba1b203603811d1be5b84, health protocol version 6.
Its `/api/connection` returns 404. The fuller dashboard lives on the feature branch;
it has not been promoted. This explains the older limited Production interface.
DNS returned A records 216.198.79.65 and 216.198.79.1; HTTPS validation succeeded.
These are observations, not DNS records customers should configure.

## Implemented dashboard and device management

The existing complete camera dashboard remains the remote control surface:
live/pause/reconnect/fullscreen/quality/digital zoom; recording/photo/light;
Saved files with authenticated bounded downloads; Activity; motion/recording/image
settings, privacy, restart and WhatsApp recipient management. Each command carries
a camera identity and request ID and waits for a physical-device acknowledgement.
New Camera Settings shortcuts expose this dashboard directly from each camera card.
The last successfully decoded frame has an explicit receive timestamp. Renaming
updates the selected dashboard name after the server confirms it.

Camera Settings → Remove Camera names the selected camera, explains the effects,
and asks for confirmation. The owner-scoped server operation atomically archives
the registration, disables it, rotates its token hash and pauses notifications.
It retains owner/hardware records, recipient configuration, event history and photos;
there is no SD command and no database DELETE. Archived cameras leave My Cameras.
Routine retention maintenance skips archived history. The existing SQL pairing
function permits a physically authorized new pairing after deliberate owner removal;
a camera-initiated factory reset still retains ownership protection.

Relay revocation closes active sockets. If the relay is unreachable, the response
and UI disclose that existing tickets expire within at most 90 seconds. The rotated
device credential immediately prevents new authenticated heartbeat, commands,
uploads or ticket issuance. No firmware delivery is needed to revoke an offline
device. A stale in-flight heartbeat conditionally writes only while enabled and
while its original credential hash still matches; otherwise it returns 401.

## Offline and installation behavior

The single normal portal remains https://vision.robetix.com. The service worker
used for secure streaming downloads also caches a small public offline recovery
shell. It never caches account lists, authentication callbacks, API responses,
JPEGs, file chunks, session credentials or recipient information. Shell content
hashes trigger worker updates; old shell caches are removed on activation.

Without connectivity, a previously visited portal can show Open local camera.
The user still has to join the physical camera's Wi-Fi. This deliberately uses a
top-level navigation to its HTTP bootstrap, not mixed-content HTTP fetch from the
HTTPS app. No public DNS lookup, camera discovery, TLS override, reverse proxy or
Internet sharing is invented. A first-ever visit without Internet cannot install
the shell; the original local address remains the bootstrap fallback.

The portal connectivity notice makes bounded reachability checks independently
of device heartbeat and browser online/offline hints. It does not redirect, replay
mutations or stop recording. The offline page verifies cloud reachability with
three bounded attempts before reopening the portal. Existing local-to-cloud
switching guards preserve recording, setup, maintenance and user activity.

The manifest and install prompt are browser-dependent; unsupported browsers can
use their Add to Home Screen menu. This does not guarantee background alarms,
offline account access, offline remote footage or identical installation behavior
on every mobile OS. Urdu preference and RTL layout persist in the offline shell.

Browser references: [mixed content](https://developer.mozilla.org/en-US/docs/Web/Security/Defenses/Mixed_content)
and [service workers](https://developer.mozilla.org/en-US/docs/Web/API/Service_Worker_API/Using_Service_Workers).

## Tests actually run

- Backend: 51 passing tests, including new owner isolation, explicit removal
  confirmation, credential rotation, archived listing, relay failure, persistence
  failure and in-flight heartbeat regression cases.
- Relay: five passing tests, including actual relay state-machine routing,
  scoped file chunks, five-camera separation, revocation and ticket expiration.
  Wrangler dry run passed; no relay deployment was performed.
- Browser tests: unified five-camera overview/removal/confirmation/cancel,
  cached offline shell without private data, offline→online recovery,
  English/Urdu layouts; existing remote controls, file downloads, JPEG decode/ACK,
  stale-frame hiding, login/session restoration, pairing and recipient management.
  Camera/provider transport is simulated; browser and application code are real.
- Local browser regressions: hotspot trust, blocked router claim, optional setup,
  local recording/photo/files/light UI, two-address switching guards and separate
  physical-maintenance tool. Device and serial transports are simulated.
- SQL fixture tests: five cameras, ownership locks, explicit transfer, preserved
  records, repeatability, rollback and access isolation. No live migration run.
- PlatformIO esp32cam and esp32cam_preview builds passed. Static RAM 80,216 bytes
  (24.5%); production flash 1,529,529 bytes, Preview 1,529,581 bytes (48.6%). Static
  RAM does not measure runtime heap, PSRAM usage or sustained frame rate.

## Remaining release gates

Re-pair the real camera after checking its installed build and Wi-Fi. The prepared
firmware environments are `esp32cam_preview` for isolated hardware acceptance and
`esp32cam` for a separately authorized Production rollout. Ordinary firmware upload
preserves NVS identity/settings; do not erase flash or factory-reset just to update.
Existing explicit paired origins remain preserved and must match the selected
environment; an upload alone does not migrate cloud ownership or pairing.

Physical acceptance must demonstrate new authenticated heartbeats, remote JPEG
decode/ACK outside the camera network, flashlight state, SD recording and finalizing,
saved-file transfers, motion, reconnect after power/Internet loss, and standalone
operation. The previous SD timeout and real-device TLS trust issue cannot be marked
resolved without retesting the hardware. Never infer video from a fresh heartbeat.
No production promotion is authorized by this implementation task.
