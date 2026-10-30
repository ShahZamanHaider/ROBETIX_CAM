# Two-address implementation and engineering acceptance

Customer instructions live in CUSTOMER_GUIDE.md. Other documents are engineering
references and historical validation, not alternate customer access methods.

The local session repair remains unchanged: actual hotspot association and
same-origin checks precede session issuance, cookie ownership survives HTTP send,
and the next request must return authenticated=true. Router devices do not gain
access from their IP address alone. Router handoff remains available only through
Advanced Diagnostics. No public API or cloud factory-reset permission was added.

The local application now hides router addresses and mDNS from everyday setup and
Settings. Customer cloud links use the shared permanent customer origin and never
fall back to a Preview address. Localhost is supported by the explicit engineering
proxy, not presented as the camera address.

CloudSwitch requires all of: direct hotspot access; authenticated local session;
idle Home; finished setup; no recording, privacy, benchmark, dialog, alarm or
pending local write; no customer interaction; a recent successful camera cloud
connection to the Production service; confirmed pairing and live-service readiness;
and a separate credential-free, bounded browser HTTPS probe. Probe responses must
identify Production, the canonical service and configured auth/live components.
After a cancellable five-second notice it probes again and rechecks every guard.
An unavailable service leaves local Home working; repeated probes are rate limited.

An explicit stay-local choice or return from the cloud remains local for the tab.
There is no cloud-to-local automatic redirect. Browser/OS network switching and
captive-portal restrictions cannot be bypassed. A reachability probe cannot promise
that the Internet will remain connected after navigation; a cloud local-access link
lets users return after joining camera Wi-Fi.

The selection fragment contains a device identifier, not a credential. It survives
ordinary sign-in in tab session storage. The cloud opens it only after the account
API returns the owned, enabled, remote-capable device. Other accounts cannot use
the selection hint to obtain commands or video. Existing cookie session restoration,
refresh and device revocation remain in place.

The public connection endpoint contains no account, device, session or secret
values. It permits credential-free browser reads from the official hotspot origin.
It must be promoted with the tested cloud code before Production auto-switching
can activate. Until then, a missing/older endpoint deliberately keeps local access.
Preview is tested independently and is never an automatic customer destination.

Required physical acceptance: first/fresh/returning browser, reload/reboot, Wi-Fi
reconnect/DHCP delay, router Internet and browser offline independently, recording
during switching attempts, WPA2 startup radio observation, working SD recording,
real remote JPEG and acknowledged controls. Compilation and simulated transport
do not establish these results. Previously observed device TLS NOT_TRUSTED and
SD timeout 0x107 remain hardware acceptance blockers. No flash erase, reset,
database migration, Production promotion or relay deployment is part of this update.

One-time firmware installation after checking the connected board/environment:

    pio run -e esp32cam_preview -t upload --upload-port COM_PORT

This installs the candidate without erasing NVS or SD data. Substitute the actual
USB serial port; do not guess it. The normal esp32cam target is Production and
must be selected only when its cloud deployment is verified and approved. Both
targets expose version/commit/environment in Advanced Diagnostics.
