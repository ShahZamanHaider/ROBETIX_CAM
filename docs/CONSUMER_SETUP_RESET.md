# Consumer access and reset — implementation status

This is a tested development branch, not a hardware-accepted market release.
Production and existing Supabase accounts/data are unchanged. No SQL migration.

## Normal local access

Join the password-protected camera hotspot and open http://192.168.4.1. The page
POSTs a same-origin bootstrap automatically. Firmware verifies the receiving AP
interface and the peer against the Wi-Fi driver's associated-station/DHCP list;
a client IP or Host header alone is insufficient. A random HttpOnly, SameSite
Strict remembered cookie is issued. There is no password, PIN or setup-code form.
The former login/provision/trust/recover/authorize HTTP routes return 410.

An unknown router browser cannot bootstrap. Join the camera hotspot first and use
the existing Open camera on home Wi-Fi link: its short-lived, single-use handoff
moves trust to the router address without a manually entered code. Cookies are
scoped to their origin; arbitrary IP changes or unrelated browsers cannot inherit
them. Four trusted local browsers are retained, with bounded slot replacement.
Local-only setup needs no cloud account. The first setup page offers Use locally;
completed setup returns to Home. SSID bytes, including trailing spaces, are kept.
The scan selector preserves exact names and status displays the actual DHCP IP.

## Factory credentials and labels

Fresh, unprovisioned units generate a stable ROBETIX-VISION-01..99 display name
from the chip identity with bounded nearby-name collision probing. By explicit
owner request, the default WPA2 password is the Wi-Fi name in ASCII lowercase
followed immediately by 786, with no extra spaces. Example: ROBETIX-VISION-01
uses robetix-vision-01786. This is predictable from the public SSID and is not
a secret-strength password policy. A custom password remains available.
The persistent 128-bit hardware tag, not the short number, is the cloud identity.
99 names cannot be globally unique. Existing labels never silently change;
nearby duplicates are reported and the hotspot name is editable in Settings.
Simultaneously provisioned or previously isolated units can still share a name.
A manufacturing allocation/label process is required for a saleable batch.

**This owner-requested update migrates existing hotspot and factory passwords once**
to the new naming rule. Router Wi-Fi credentials, mode, setup progress and cloud
ownership remain intact. Existing printed factory labels must be replaced.
Ordinary restarts preserve subsequent explicitly customized passwords. Renaming
an automatically named hotspot updates its derived password unless a custom
password is supplied. SSIDs shorter than five bytes need an explicit password
to meet WPA2's eight-character minimum.
Factory credentials remain in rv_factory, separate from resettable robetix
preferences. This namespace separation is not flash encryption.

The USB recovery page shows the private factory label on explicit request,
without Serial Monitor or terminal commands. Print/supply that label privately
before shipping. Do not put credentials in Git, cloud dashboards or plaintext
operator notes. Keep any additional administrator credentials in a password manager.

## Reset behavior

Settings → Device → Factory Reset uses a normal confirmation dialog. The browser
must have a valid session AND be physically connected through camera Wi-Fi.
Router and cloud clients cannot request a hardware factory reset. The recorder
must finalize successfully first; otherwise reset is cancelled. Retention work
is paused. The cloud worker attempts credential revocation over verified HTTPS.
The backend rotates only that device credential and clears last_seen, preserving
its enabled ownership claim, users, settings, recipients and media. It does not
mark the record disabled: doing that would release the hardware for takeover.

If offline or unverified, reset proceeds locally and records pending revocation
and owner review in the factory namespace. No old bearer credential is retained
for background retries. The existing cloud owner must Disconnect the old camera
from their signed-in account; the hardware claim prevents another account from
claiming it before that owner-authorized release. This is accurately shown as
pending, not silently reported as successfully unlinked.

Reset clears the robetix namespace (home Wi-Fi, custom hotspot, local sessions,
admin credentials, camera/motion/recording/cloud preferences). It retains factory
credentials/hardware identity and SD files. A persistent resetPending marker
makes the next boot repeat cleanup before workers start, preventing old in-flight
writes or interrupted resets from restoring settings. Firmware remains installed.
No SD format is coupled to reset; existing explicit file deletion is separate.

## Physical USB recovery

Open recovery.html in desktop Chrome/Edge on HTTPS, connect the USB base and click
Connect camera → Factory Reset → Confirm. Web Serial requires a browser USB port
permission. A fresh 30-second nonce is exchanged internally; users copy no codes.
The USB command only queues the same safe capture-worker reset. The page waits
for a real post-restart ready marker before claiming completion. It cannot reset
old firmware that lacks this protocol: installing this update once is necessary.
The recovery page makes no API requests and stores no credentials. The same
serial connection can show the factory setup label privately. No flashing occurs.

## Validation and release gates

Backend tests cover credential revocation, retained ownership and preserved data.
Browser tests cover automatic hotspot entry, denied unknown-router access,
remembered router access, AP-only reset, first-use and local-only setup, RTL,
private label and the USB handshake/reboot race with simulated serial transport.
Host tests cover the bounded number allocation, lowercase-name password rule and buffer limits.
PlatformIO builds both esp32cam (permanent production origin) and esp32cam_preview
(ignored pinned Preview configuration). Secret scans include staged files/history.

Still required before market acceptance: actual first-boot/AP authorization and
router handoff, physical reset/reboot including interrupted reset, USB recovery,
physical certificate-chain repair and fresh remote JPEG, working SD card/AVI,
repeated motion and long-duration mixed recording/network/memory tests. Mobile
background audio remains subject to browser/OS restrictions. No current hardware
FPS, audible-speaker, SD or remote-video pass is inferred from simulated tests.
