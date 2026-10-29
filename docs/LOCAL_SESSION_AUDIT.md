# Local session audit — 29 October 2026

Development candidate; physical acceptance is still required. This document
supersedes the older password-policy and four-browser descriptions in
CONSUMER_SETUP_RESET.md.

## Reproduced defect

The hotspot handler destroyed its local Set-Cookie String before reply() sent
the response. ESP-IDF borrows header pointers until sending. AP membership could
therefore pass while the browser received no usable cookie. Both hotspot and
address-handoff responses now retain the String through send and fail closed on
allocation, storage or header errors. The native regression extracts the actual
handler, reproduces the old expired pointer, then verifies the repaired code.

The browser verifies authenticated=true after bootstrap, deduplicates concurrent
requests, and limits recovery to three attempts within twelve seconds. Rejected
cookies do not cause repeated session grants. Temporary outages retain Home in
an offline state. Mutations are never automatically replayed. Origin handoff
reuses the existing session rather than consuming another trusted-browser slot.
Eight bounded slots preserve existing stored sessions.

## Provisioning and maintenance

Fresh units receive a stable ROBETIX-VISION-01..99 name with nearby collision
probing and a random eight-digit numeric WPA2 password. Short names cannot be
globally unique. Factory credentials must be supplied on a private setup label.
Installed credentials do not change on upgrade. Explicit USB-only label
provisioning can prepare a new random factory password without changing current
Wi-Fi or cloud settings; it applies after an explicit factory reset. The operator
must privately supply the replacement label before resetting.

Factory reset remains authenticated and hotspot-only, finalizes recording,
preserves SD files and factory identity, and retains cloud ownership protections.
Physical maintenance is separate from ordinary customer screens. Generate its
private offline page with tools/package-maintenance.py; no device credentials
are embedded in the saved page.

The AP is configured while stopped, with WPA2 installed before radio startup.
The previous Arduino call order could expose a startup configuration window.
This source finding does not identify every observed ESP_ network; a physical
radio trace after reboot remains necessary.

## Development and version identity

Localhost means the computer. For development, explicitly run:

    node tools/local_camera_proxy.mjs --camera http://192.168.4.1

Open http://127.0.0.1:8080 while connected to camera Wi-Fi. The loopback-only
proxy serves the real UI, forwards only the camera cookie, validates browser
origins, and does not grant camera authorization. Ordinary users open the actual
camera address. Cookies do not move between IP addresses automatically; use the
trusted camera page's home-Wi-Fi link.

Advanced Diagnostics shows firmware version, commit, source identity and service
environment. UI and device source identities are compared. Firmware upgrades
preserve the explicitly paired cloud origin instead of silently moving device
credentials between Production and Preview.

## Verification limits

Host tests cover cookie lifetime, real Auth source with mocked storage, secure AP
startup order, bounded browser recovery, motion rearming and network policies.
Browser tests cover real cookie persistence and UI behavior with simulated
device responses. Backend and relay tests use simulated device traffic.

The latest physical attempt could not reach the hotspot; the computer's original
Wi-Fi was restored. No firmware upload, reset, SD formatting or database change
was performed. The supplied session JSON proves AP detection but contains no
build identity, so the installed commit cannot be confirmed. Reflash without
erasing NVS, then validate fresh/returning browsers, reload/reboot, router handoff,
local video/controls, SD recording and fresh remote JPEG. No market-readiness or
physical streaming claim follows from software tests.
