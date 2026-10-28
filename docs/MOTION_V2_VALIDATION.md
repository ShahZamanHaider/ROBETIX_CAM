# Alarm, motion v2 and bilingual interface — validation

Date: 2026-10-28. Starting checkpoint: `checkpoint/pre-motion-v2` at `c306a41`. This is an in-place upgrade. No ESP32 serial device was available, no firmware was uploaded, and no physical speaker/phone listening test was performed. Existing native-JPEG capture, AVI writer, storage, network, authentication and optional cloud service remain in place.

## Exact causes and fixes

**Alarm:** the old controller stored native `setInterval` on its instance and invoked it as `this.interval(...)`. Real Edge reproduced `TypeError: Illegal invocation` after the initial short tones: the browser rejected the receiver and never installed the repeating scheduler. Its active flag survived. Mock timer tests had not reproduced browser host-function behavior. Even with that exception fixed, short tones replenished on a JavaScript timer could run out under throttling.

`data/alarm.js` now wraps native timers with the correct global receiver and uses one original two-second continuous siren buffer rendered by a looping `AudioBufferSourceNode`. Sound does not depend on timer replenishment. One AudioContext is reused; stop/mute release the source and gain; duplicate events cannot create overlapping sources. A low-rate timer handles explicitly selected timed mode/snooze, not the siren waveform. Context state changes, zero volume, mute and source lifecycle determine playback status. Suspended sound asks for another enable tap rather than claiming to ring. The local UI also preserves the stored unacknowledged alert during initial settings restoration.

**Motion:** the previous detector rearmed only after 30 uninterrupted quiet seconds. Subsequent movement or background residuals reset its quiet timer, keeping normal repeated hand entries in the same latched event. The default was unsuitable for this interaction.

`src/MotionCore.h` now separates actual frame activity, new foreground, known foreground occupancy and event timing. Monitoring → Possible → Confirmed → Event Active → Motion Ended → Ready → Monitoring is explicit. About 750 ms of sampled frame stability ends an event; the minimum start interval is measured from the previous event start, independently of later motion. Returning to the reference clears known occupancy. Defaults are one-second confirmation and a two-second minimum start interval. Configurations without the new `motion_v2` marker migrate once to these timing values. A selected longer interval still intentionally delays events. Stop Alarm changes only browser audio state and does not disable firmware monitoring.

## Detection and resource implications

- Fixed 64×48 luminance grid from scaled JPEG decoder blocks, with no full-resolution RGB allocation and no JPEG-size heuristic.
- Adaptive background/noise, connected neighbors, 4×4 regional scoring, optional smaller connected clusters, global brightness compensation and limited one-cell camera-shift rejection.
- Sampling remains bounded, default 500 ms. The single existing capture task shares frames with streaming/recording. Slow SD writes skip analysis; decode callbacks retain their 100 ms budget. Above SVGA, analysis is explicitly skipped.
- Core storage increases by roughly 6 KiB of PSRAM; the isolated test core costs roughly 22 KiB while testing. Arrays are fixed. Test storage is freed on stop/expiry. The same sampled decode feeds whichever detector is selected.
- `algorithm_ms`, `decode_ms`, skipped samples and workspace size report actual device work. No ESP32 processing time, FPS, detection distance or long-term stability is established by host tests. Validate the tradeoff on the actual camera/card.
- Test mode never generates motion events, photos, motion recordings, cloud alerts or sound. Manual and explicitly selected Always recordings remain independent. Passive Show Movement Areas does not activate test mode.
- Lighting, shadows, wind, reflections, contrast, sub-grid objects, faster motion than the sampling interval and camera movement can still defeat these heuristics. This is motion/foreground analysis, not person, animal or face recognition.

## User interface and languages

Settings exposes Motion Detection, Low/Normal/High sensitivity, Detect Smaller Movements, Record When Movement Happens, Ring Alarm When Movement Happens, Show Movement Areas and Test Motion Detection. Detailed parameters are nested under diagnostics. Test status uses simple words and coarse highlighted regions. Stop Alarm stays independent of monitoring.

`data/ur.json` contains 629 compact Urdu translations shared by local and optional remote pages. `data/i18n.js` handles static text, parameterized dynamic messages, validation errors, accessible labels and localized confirmation/input dialogs. It keeps original text for immediate switching back to English. Browser-local preference survives reload; Urdu applies RTL and system fonts without external downloads. User/device names, filenames, URLs, Wi-Fi names, pairing values and required reset strings retain their original values. Generated firmware UI and remote translation bundle come from `tools/embed_ui.py`.

## Software evidence

Final PlatformIO release build: **PASS**, Espressif32 6.11.0 / Arduino-ESP32 2.0.17 / ArduinoJson 6.21.5. Static RAM **71,340 / 327,680 bytes (21.8%)**; flash **1,407,033 / 3,145,728 bytes (44.7%)**. These static figures exclude runtime heap/PSRAM allocations. No new firmware library dependency was added.

Firmware binary SHA-256: `35CE55B3BBFBC3B7B466E6EFC03C1A5EFEED121EFFAB72C2BDDDCB5AEB17B70E`.

| Test | Result and boundary |
|---|---|
| `tools/test-alarm.cjs` | PASS: looping waveform has no silent windows; frozen JS scheduler does not replenish/stop it; duplicate events, stop/rearm, zero volume, mute, snooze, timed mode, restoration, suspended/closed context recovery. Mock lifecycle evidence only. |
| `tools/test-alarm-browser.cjs` | PASS in actual headless Edge Web Audio: **61.2 seconds of decoded digital output**, minimum sampled per-second RMS 0.050929; post-stop analyser RMS 0; graph released; no duplicate source; rearm and suspend/resume pass; tab switched away and back. This measures rendered digital signal, not human hearing or phone hardware. |
| `tools/test_motion_v2.cpp` | PASS: three synthetic hand entry/exit cycles produce three events, continued motion stays one event, rearm and custom interval, static/noise/global lighting and coarse camera shift. Synthetic grids do not establish field accuracy. |
| `tools/test_core.cpp` | PASS: existing schedule, confirmation, cooldown, noise, illumination, small-cluster and excluded-zone regressions. |
| `tools/test-i18n.cjs` | PASS: 629 entries, dynamic/API error coverage and preserved filenames/URLs. |
| `tools/test-ui.cjs` | PASS with mocked APIs in Edge: existing controls, four local pages, English/Urdu, mobile/desktop RTL, translated dialogs/errors, test start/stop, saved preference and alarm restoration. No missing keys in exercised flows. |
| `tools/test-remote-ui.cjs` | PASS with mocked APIs in Edge: Urdu login, names/activity/pairing, translated revoke confirmation, mobile RTL, language persistence and no missing keys in exercised flows. |
| `node --test remote/test/*.test.mjs` | PASS: six existing service suites. Database and Meta are mocked; this is not a deployed-provider test. |
| `tools/test_avi.py` | PASS: unchanged C++ writer finalizes 15 JPEG frames; short writes fail safely; RIFF/index lengths/offsets and embedded JPEG decode checked. Full AVI demux/decode was not run because PyAV is unavailable. |

Browser artifacts are ignored local test outputs in `.pio/ui-test/results/`, including `alarm-60-seconds.webm`, `alarm-audio.json`, and Urdu layout screenshots. Camera images in UI tests are synthetic fixtures. No physical camera stream was tested. Desktop Chrome, Android/iOS, locked-screen audio and live ESP32 endpoints remain unverified.

## Exact device acceptance sequence

1. Build/upload using the README commands and your real COM port. Keep the checkpoint available. Log in with the existing administrator account; verify both configured AP/STA paths and preserved settings. Check sensor reports OV2640/native JPEG, PSRAM is available and SD is ready. Do not format the card.
2. Use VGA/Normal, motion On, Normal sensitivity, default confirmation/interval, automatic recording Off initially. Let an empty stationary scene settle for at least three seconds. Enable alarm sound with a tap and confirm phone/browser volume is audible.
3. Enter the frame with a hand; hold it visibly for at least two seconds to allow sampled confirmation. Leave and allow 2–3 seconds of stable scene. Repeat three times. Expect three separate history events when contrast/thresholds permit. A very short movement or insufficient stable separation may legitimately remain one event. Test High/Detect Smaller Movements for smaller subjects.
4. During the first event, listen for **at least 60 seconds** without pressing Stop. Verify actual sound throughout. Trigger another event while ringing: history advances with one sound loop. Press Stop Alarm and verify immediate silence while Motion Detection remains On. A later distinct event must ring again. Test Mute and Test Alarm separately.
5. Switch tabs and return; compare displayed playback state with actual sound. Suspend/resume where possible, reload an unacknowledged alarm, and re-enable sound with a tap. Test desktop Chrome and the intended phone browser. A locked phone/backgrounded browser may suspend audio; continuous background operation cannot be promised.
6. Start Test Motion Detection. Repeat hand entries and exits, watch movement highlights/normal state, and verify event count, photos, motion recordings and cloud alerts do not increase. Stop the test and verify normal event monitoring resumes. Existing manual/Always recording is intentionally not stopped by the test.
7. Repeat with manual recording plus preview, and with movement recording. Stop/finalize/download AVI and play it in a compatible player. Confirm only one recorder operates and downloads/deletion retain their busy guards. Test card absent/full/write failure without claiming these were simulated electrically here.
8. Observe a static scene, switch room lights, move a small distant subject and gently disturb the camera. Record false positives/misses rather than assuming perfect rejection. Compare algorithm/decode timings, skipped samples, capture/SD latency, free heap/PSRAM and real recording FPS with motion Off/On and test On. Lower resolution/sampling load if recording suffers.
9. Switch English/Urdu without restarting: Home, Saved videos/photos, Activity, Settings/Advanced, setup/login, Wi-Fi, flashlight/zoom, cleanup/date-delete, errors and all destructive confirmations. Check names, dates, URLs and paths remain legible on your phone. Reload to verify preference. Cancel destructive dialogs during this check.
10. Run for several hours/overnight with your chosen recording mode; verify repeated movement, stable memory, finalization, recovery after Wi-Fi loss and selected post-power-restoration behavior. Power loss may still leave the last AVI `.part` incomplete, as documented previously.

Existing undeployed cloud/WhatsApp integration still needs Vercel, Supabase, Meta configuration and real end-to-end tests. Face/person recognition, interrupted AVI recovery, unrestricted background phone alarms and claims of unlimited-distance detection remain outside this upgrade.
