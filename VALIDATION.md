# Validation and device acceptance

Latest alarm/motion v2 and English/Urdu evidence: [docs/MOTION_V2_VALIDATION.md](docs/MOTION_V2_VALIDATION.md). It supersedes earlier motion timing and alarm implementation details below and records the current build sizes and device acceptance steps.

The professional upgrade adds persistent alarm, 64×48 motion analysis, native OV3660/OV5640 acceptance, supported-range light/zoom controls and optional cloud source. See `docs/PROFESSIONAL_TESTS.md` for its additional tests, and `remote/README.md` for account/deployment requirements. Baseline evidence below is historical where sizes/features differ.

## Evidence from this workspace

- PlatformIO ESP32-CAM builds passed at the essential-recording checkpoint, motion checkpoint, and integrated product checkpoint. Final artifact is `.pio/build/esp32cam/firmware.bin`. See final build result in the delivery message for current linked size. Static RAM is not runtime free-heap evidence.
- `node tools/test-ui.cjs` passed using installed Edge and Playwright with simulated firmware responses: initial login, remembered Home entry, fresh preview/stale-frame hiding, recording/stop/save states, photo requests, file links/deletion, picture settings, shared Settings preview, AP/STA fields, safe SSID rendering, 390px mobile layout, missing-card disabled actions, flashlight/motion requests, confirmed date-deletion API and logout. No physical camera footage was used.
- Portable C++ checks passed with assertions enabled: invalid-clock scheduling, overnight weekday ownership and end boundary; motion noise/global-light rejection, consecutive confirmation, cooldown and excluded zones.
- The same C++ AVI writer finalized 15 real JPEG frames and rejected an injected short write. Python verified RIFF sizes, every index offset/length, and decoded all embedded JPEGs with Pillow. **Full AVI demux/playback through FFmpeg/PyAV was not run:** dependency download failed with DNS/network errors. Real SD AVI playback still requires the device test below.
- Pre-professional checkpoint build: SUCCESS; static RAM 72,712 / 327,680 bytes (22.2%); application flash 1,112,149 / 3,145,728 bytes (35.4%). These are linked sizes, not runtime memory measurements.
- No serial ESP32 was detected; no firmware upload, sensor identity measurement, SD recording, runtime memory measurement, thermal test or overnight run occurred.

## Flash and first boot

1. Preserve the current firmware/settings/card contents if needed; source backup is `backups/phase1-source.zip`. Power off before inserting a known-good FAT32 microSD or reseating the OV2640 ribbon. Do not format a card containing needed data.
2. Build with `platformio run`; flash with `platformio run -t upload --upload-port COMx`. Release GPIO0 from ground and reset. Open the serial monitor at 115200 baud.
3. Save the complete boot log. Require sensor PID `0x0026`, native JPEG initialization, PSRAM size, and SD one-bit mount/card capacity logs. If either initialization fails, preserve the named error and test power/cable, ribbon orientation/seating and a known-good FAT32 card. Firmware cannot repair an electrical or incompatible-card fault. Brownout remains enabled.
4. First boot: join Robetix-Setup / RobetixSetup and open http://192.168.4.1. Set an administrator password, then select AP or STA and your own Wi-Fi credentials. Verify saved AP credentials, router discovery, recovery hotspot, IP access and robetix.local where supported. Switch the router off/on and verify bounded reconnection.
5. Log out; verify `/api/status`, `/api/preview`, `/api/files`, `/api/events`, `/api/cleanup`, action routes and port-81 downloads reject unauthenticated access. Log in with Remember enabled, reboot and revisit: Home should open. Sign out revokes that token. Test a nonremembered session separately. The setup hotspot password is not the administrator password.

## Essential CCTV — pass before automatic operation

6. Watch Home for 10 minutes at Normal. Move a visible clock/object to confirm fresh frames. Disconnect phone Wi-Fi: stale picture must hide and Offline/reconnection must appear. Reconnect; check Settings uses the same preview and no second capture loop. Test portrait/landscape, fullscreen, fit, zoom/reset and preview collapse.
7. Take three photos, browse Photos, open/download each and verify actual JPEGs. Confirm capacity/free space changes and timestamp is accurate after browser sync. Without a trustworthy clock, files should be undated rather than assigned a fabricated date.
8. Record 15 seconds, Stop & Save, and download the AVI. Open with VLC or another MJPEG AVI decoder. Verify duration, moving frames, image dimensions, index/seeking and end-of-file. Repeat 30 clips, including clips with simultaneous viewing. There must be no duplicate sessions, leaked handles, unexpected resets or steadily falling minimum/free heap. Save diagnostics before/after.
9. Repeat at Low, Normal and Best with recording ceilings 3 and 5 FPS first. Record capture_ms, write_ms, actual AVI frames/duration, heap, PSRAM, skipped motion samples, preview drops and errors. Use only measured sustainable settings for daily use. Run the separate camera benchmark while recording is stopped; it temporarily owns capture exclusively. Higher resolutions need additional testing and may exceed preview limits.
10. On a backed-up disposable test card, exercise insufficient free space, failed writes/card problems, and loss of power during recording. Never assume the last `.part` is playable. Completed prior clips must remain readable; unfinished files must remain excluded from catalog/cleanup. Power off before physically removing/reinserting the card. No-card boot must still allow configuration/live view.

## Motion and unattended operation

11. Enable Motion Alerts with photo only. Test a static scene, minor lighting noise, a real moving subject, a large illumination change and the selected zone boundaries. Confirm consecutive detections/cooldown and measure decode_ms. It detects movement, not people. Repeat while viewing, while idle QVGA monitoring, and while recording; count missed/false events.
12. Select Record on Movement. Confirm one recording starts, activity links only a finalized video, and manual Record cannot create a competing session. Stop must disable automatic mode. Test unavailable/full SD and retry behavior; no reset loops should occur.
13. Select Always Record, reboot, and verify mode resumes after camera/card initialization. Confirm bounded clip rotation and expected gaps. Test Time-Lapse separately; downloaded playback should be 5 FPS while frame content advances at the configured interval. Do not combine it with Movement mode.
14. Save an overnight schedule using correct phone timezone. Test before/start/end boundaries and selected weekdays. Reboot offline without a browser or NTP: scheduling/retention must wait for clock sync. Resync and verify operation resumes. Resave after daylight-saving offset changes.
15. Enable alert sound by tapping, Test/Stop/Mute/Snooze, then trigger motion. Verify in-app messages and event history. Lock/close the browser to establish the platform's limitations; no background alarm guarantee is made.

## Storage, reset and GPIO checks

16. Use expendable files for deletion tests. Search a known date range; verify count, total size, earliest/latest dates before confirming. Test reversed dates and no matches. Add a new file after searching; it must not join the confirmed batch. Check individual/multiple-page selection deletion. Malformed names, traversal paths, active `.part`, undated and unrelated files must not be deleted by date/retention policies.
17. Enable retention separately for old photos and videos, with correct clock. Verify only eligible finalized Robetix files are removed while idle; disabling it stops future cleanup. Test free-space cleanup separately: only oldest completed videos, at most eight per attempt. A pending date-search confirmation pauses retention; restart/research as needed. Logs are bounded RAM history.
18. Test flashlight 0/40/100% of the supported range (electrical duty 0/10/25%) during preview, SD photo and recording. Verify no SD corruption, XCLK disturbance, resets or unacceptable heating. It must turn off within 60 seconds and immediately on Privacy/reset. The cap is provisional; reduce output if the board heats or supply sags.
19. Pause Camera during recording: file finalizes, image hides, light turns off and capture stops. Reboot paused and resume. Test Camera Reset keeps Wi-Fi/files; Factory Reset with typed confirmation clears ownership/network/settings, leaves card media intact, and returns to setup. Do not use Factory Reset on an unbacked-up installation until this behavior has been checked.
20. Only after these pass, run a supervised 1-hour test then an overnight test using the chosen modes. Log restarts, free/min heap, PSRAM, SD write errors, stream drops and actual clip FPS. Inspect representative beginning/middle/end clips. This is required before claiming unattended stability.

## Host checks

Host-only dependencies: Zig C++ compiler, Pillow, PyAV, Playwright with Edge. They are not firmware dependencies.

```powershell
.pio/host-tools/ziglang/zig.exe c++ -std=c++17 -O1 -nostdlib++ tools/test_core.cpp -o .pio/test_core.exe
.pio/test_core.exe
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" tools/test_avi.py
node tools/test-ui.cjs
```

The AVI test uses the same templated C++ writer as firmware, injects a short write and checks every index offset/length and RIFF size. With PyAV installed it also demuxes/decodes the AVI; without it the test explicitly reports that stage as not run and decodes embedded JPEG payloads using Pillow. This verifies the portable container implementation, not ESP32 File/SD timing or real camera JPEG compatibility. Motion/schedule checks use synthetic grids and timestamps, not real-world detection accuracy.
