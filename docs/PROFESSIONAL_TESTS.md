# Professional upgrade acceptance tests

## Software evidence

The baseline is Git commit `55fdd71`; its build passed before new edits. Later checkpoints compiled the persistent-alarm/UI/motion/sensor changes and the optional HTTPS cloud agent. Final build on 28 October 2026: **SUCCESS**, static RAM **71,180 / 327,680 bytes (21.7%)**, flash **1,328,261 / 3,145,728 bytes (42.2%)**. These are linked sizes, not runtime free-memory measurements. Firmware SHA-256: `525ec89365115b23c1877f00cd102c877fba67ac9ed38a29a1867728e70d44e6`. No serial device was detected; **no physical acceptance result is asserted**.

Executed checks:

- `node tools/test-alarm.cjs`: simulated ten minutes of a manual alarm, event deduplication, bounded nodes, explicit Stop/rearm, mute, snooze, suspended audio and timed mode.
- `node tools/test-ui.cjs`: actual headless Edge with simulated local APIs. Existing login/record/stop/photo/file/network/date-deletion/mobile regressions, remembered Home entry, shared settings preview, supported brightness range, Home zoom/reset, cloud Not configured, and Web Audio Test/Stop behavior. Browser screenshots use a tiny synthetic fixture, not camera footage.
- Native `tools/test_core.cpp`: assertion-enabled C++ synthetic grids and schedule timestamps. Global lighting/noise rejection, confirmation, cooldown, excluded zone, 2×2 changing-cell cluster at High sensitivity and isolated-cell rejection. These are deterministic algorithm cases, not a field accuracy benchmark.
- `tools/test_avi.py`: the same C++ AVI writer finalized 15 real JPEG payloads, rejected an injected short write, and passed RIFF sizes/index offsets and embedded JPEG decode checks. Full FFmpeg/PyAV AVI demux remains unrun because dependency download failed; physical AVI playback must be tested.
- `node --test remote/test/*.test.mjs`: six mocked-provider integration suites: validation/time/templates/HMAC; private JPEG plus exactly one media template submission; unknown device/Off/opt-in/rate rejection; ambiguous submission deduplication; account ownership/photo authorization/revocation; signed delivery state update. The real PostgreSQL RPCs, Supabase Auth/Storage policies and Meta API have **not** been exercised.

## Local device checks before cloud

Run the original `VALIDATION.md` sequence first. Preserve boot logs showing actual sensor model/PID, PSRAM and SD mount/card information. Resolve ribbon, supply, card-format or unsupported-module faults using the actual named error; no software-only test proves they are fixed.

1. Test OV2640 first. Confirm native JPEG and actual moving frames, record 15 seconds, stop/finalize, download and play AVI. Verify new settings do not break snapshots, persisted recording modes, privacy, resets, SD browsing/date deletion/retention and AP/STA provisioning.
2. Enable alarm sound by a tap. Test an alarm for at least five minutes without interaction; it must continue until Stop. Trigger additional events while ringing; there must be one loop and an updated counter/history. Stop, wait the configured event separation, then create distinct motion and confirm rearm. Repeat with volume zero, each tone, mute, 5/15/30-minute snooze and explicit timed mode, including restoration of an unacknowledged visual alert.
3. Put the page in background, lock the phone and suspend/resume browser audio. Confirm the interface discloses restrictions and a user tap restores playback where permitted. Do not count closed/sleeping-page silence as a guaranteed alarm capability.
4. Run Test Motion Detection. Confirm coarse changed regions align with the **contained image**, including a portrait/landscape screen and letterboxed picture. Draw/clear a zone and move inside/outside it. Stop calibration and verify detailed region reporting expires. Test motion Off calibration: it must not save events or start recording.
5. At each of Low/Normal/High/Custom, test small, medium and large visible targets at measured distances, with fixed lighting and then illumination changes. Include quiet scenes, shadows, reflections, wind and exposure settling. Record false alarms and misses, rather than adjusting until one demonstration passes.

| Scene / target | Distance | Approx. target pixels | Profile / interval | Local score | Confirm latency | Events / misses / false alerts | Decode ms / heap |
|---|---|---|---|---|---|---|---|
| Near | Fill on device | Fill on device | | | | | |
| Middle distance | Fill on device | Fill on device | | | | | |
| Distant | Fill on device | Fill on device | | | | | |
| Static / lighting change | — | — | | | | | |

6. Repeat detection while viewing and while recording at Normal/5 FPS. Log capture time, AVI frames/elapsed time, SD write time, decode time, skipped analysis, internal heap/largest block and PSRAM. Test no-view QVGA monitoring and resumption of the chosen view profile. No measured distance/FPS is provided by this upgrade.
7. Verify Home zoom/pinch/reset/fit only alters display, preserves aspect ratio and never crops the default frame. Check the same preview in Settings and disabled controls during recording.
8. Test flashlight supported-range 0/40/100% (currently 0/10/25% electrical duty) with the 10-second timeout first. Confirm immediate slider response, saved preference, timeout, power-cycle default Off and privacy/reset Off. Repeat SD writes and check supply stability and actual component temperature with suitable external instruments. Longer timeouts require supervised validation. Do not raise the duty cap or claim continuous full output without measured electrical/thermal qualification.
9. Only when electrically/physically compatible, power down and test OV3660 and OV5640 separately. Confirm driver model, safe native JPEG, advertised size changes, restored controls, AVI and motion. Test an unsupported module: clear unsupported error, no fake functioning stream/settings. The three accepted models are code support, not hardware-certified compatibility.

## Cloud staging tests (after you create accounts)

Follow `remote/README.md`. Use a dedicated evaluation project, the schema, server environment secrets, private storage and a real HTTPS root. Complete these tests before exposing a production account/domain:

1. Run real SQL and inspect RLS grants/RPC behavior. Anonymous and ordinary authenticated Supabase clients must not read `rv_*` tables or `rv-media` objects. Race two requests for one pairing code; exactly one claim may succeed. Test expiry and rate limiting. Check codes/tokens never appear in logs, public responses or built frontend assets.
2. Log in to the remote app, pair through the local admin page and enable remote snapshots. Test correct, expired and wrong certificates, wrong hostnames, clock unset, DNS failure and revoked device token. Never bypass TLS to make a test pass.
3. Trigger motion and view last seen, activity and actual private photo from another network. Try another account, unauthenticated requests, guessed event IDs and Remote Off. No private photo should be returned. Revoke on the remote app, verify the old credential fails, and re-pair with a new code.
4. Verify local switches stop future uploads and that an offline disable is reconciled after connectivity returns. Revoke server access while the camera is offline to enforce server-side disconnect. Existing downloads cannot be recalled.
5. Configure both approved Meta templates and webhook credentials. Opt in the recipient, send a real test image and text-only alert, and check Meta submission ID then a signed delivered receipt. Test wrong template/version/token, invalid recipient and receipt signature. Submitted must never be labelled delivered without a receipt. New sent receipts must not downgrade an already delivered/read state.
6. Disconnect Internet and simulate provider timeouts while recording for at least an hour. Local AVI/photos/motion must continue; cloud queue stays one slot, retries remain bounded, memory returns to baseline and request latency never owns the camera/SD lock. Verify ambiguous Meta submission is not automatically duplicated.
7. Test 1-day cloud retention with dated disposable objects, the 12-item bound, maintenance cleanup and configurable age access restrictions. Inspect actual storage deletion and quota dashboards; cleanup failures must not create public links. Confirm SD retention is independent.
8. Deploy the new Vercel project and configure only the exact DNS records for `vision.robetix.com` supplied by Vercel. Confirm the main website and portfolio records are unchanged. Test login/photo access on HTTPS from mobile data.
9. Run a supervised local one-hour test, then overnight with the intended cloud/alarm/recording mix. Inspect representative clips and actual resets/errors/heap trends. Cloud costs, template approval, privacy notices, backup/revocation procedures, MFA UX and independent security review remain deployment responsibilities; this release does not certify production readiness.
