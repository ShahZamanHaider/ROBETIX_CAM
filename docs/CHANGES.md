# Files changed in the professional upgrade

## Dynamic WhatsApp recipients follow-up

Compared with `checkpoint/pre-whatsapp-recipients` (`dcbf907`). No runtime npm/firmware dependency was added. The local PostgreSQL/WASM test uses the ignored development-only `.pio/sql-test` PGlite installation.

- `remote/migrations/002_recipients.sql` (new): encrypted-contact fields, delivery ledger, private table permissions, bounded/locked contact saves, one-time legacy import, cooldowns and per-recipient leases.
- `remote/lib/recipients.mjs` (new): AES-GCM/HMAC phone protection, validation, recipient-aware fanout, one shared media upload, safe provider failures, daily attempt cap and aggregate results.
- `remote/api/recipients.mjs` (new): owner-only list/add/edit/enable/disable/remove/pause/test actions; no arbitrary test target.
- `remote/api/health.mjs` (new): nonsensitive public health endpoint.
- `remote/lib/service.mjs`, `remote/api/device.mjs`: backend-managed recipient protocol, safe legacy migration, heartbeat isolation, existing authenticated native-JPEG upload pipeline preserved.
- `remote/api/webhook.mjs`, `remote/api/maintenance.mjs`: individual monotonic receipts, legacy receipts and stale-worker reconciliation.
- `remote/public/index.html`, `remote/public/app.js`, `remote/public/app.css`, `remote/public/recipients.js` (new): mobile English/Urdu contact management, consent, tests, pause, deep links and data clearing on sign-out/authentication expiry.
- `src/Cloud.cpp`, `data/index.html`: hosted management link replaces local single-number form; recipient details removed from camera status; old number cleared after migration acknowledgement; verified TLS and bounded capture-independent worker retained.
- `data/i18n.js`, `data/ur.json`, `src/Ui.h`, `remote/public/i18n.js`: shared bilingual resources, provider error translation and generated assets.
- `remote/.env.example`, `remote/vercel.json`, `remote/schema.sql`: server encryption/budget settings, function duration and migration instructions.
- `remote/test/service.test.mjs`: 17 API/provider regression suites with mocked upstreams.
- `tools/test-recipients-sql.mjs` (new): actual PostgreSQL/WASM migration, permission and RPC behavior checks.
- `tools/test-recipients-ui.cjs` (new), `tools/test-remote-ui.cjs`, `tools/test-ui.cjs`: browser recipient CRUD/privacy/RTL tests plus existing local/remote regressions.
- `README.md`, `VALIDATION.md`, `docs/CHANGES.md`, `remote/README.md`, `remote/RECIPIENTS.md` (new): operator migration/setup, evidence, limits, current cost references and remaining live-account acceptance.

## Alarm, motion v2 and English/Urdu follow-up

Compared with checkpoint `c306a41` (`checkpoint/pre-motion-v2`). No firmware library dependency added. New host/browser tests reuse the existing ignored Zig/Playwright tool installations. Generated files are included so firmware/remote assets remain reviewable.

- `data/alarm.js`: timer receiver fix, continuous single-source siren, playback lifecycle and recovery.
- `data/index.html`: simple motion controls, isolated test/areas subscriptions, alarm restoration, language selection, RTL and translated confirmations.
- `data/i18n.js` (new), `data/ur.json` (new): shared translation engine and 629 English/Urdu entries.
- `src/MotionCore.h`: foreground/event state machine, local regions, stability/rearm, noise/light/limited jitter handling.
- `src/Motion.cpp`, `src/Motion.h`: bounded test detector, passive visualization, state/timing diagnostics.
- `src/AppOptions.h`, `src/Engine.cpp`: motion defaults/migration, smaller-movement option and test action isolation.
- `src/Web.cpp`: authenticated test start/stop and movement-area subscription endpoints.
- `tools/embed_ui.py`: embeds shared language resources and generates the remote bundle.
- `src/Ui.h`, `remote/public/i18n.js` (new): generated local UI/remote translations.
- `remote/public/index.html`, `remote/public/app.js`, `remote/public/app.css`: shared language selection, translated dynamic messages/dialogs and RTL.
- `tools/test-alarm.cjs`: expanded single-source/lifecycle regressions.
- `tools/test-alarm-browser.cjs` (new): real Edge 60-second rendered digital audio capture and stop/rearm/suspension checks.
- `tools/test_motion_v2.cpp` (new): repeated entries, event separation, noise/light/jitter regressions.
- `tools/test-i18n.cjs` (new), `tools/ui-source.cjs` (new): dictionary/API-error coverage and shared test asset expansion.
- `tools/test-ui.cjs`, `tools/test-remote-ui.cjs` (new): local/remote regression and English/Urdu responsive layout checks.
- `README.md`, `VALIDATION.md`, `docs/MOTION_V2_VALIDATION.md` (new), `docs/CHANGES.md`: current behavior, root causes, exact evidence, resource limits and remaining physical acceptance steps.

## Earlier professional upgrade

Baseline: `55fdd71`. Generated assets remain derived from `data/`; the `.pio` build/test tools are ignored. No credential file, cloud deployment, DNS record or main website was changed.

- `.gitignore`
- `README.md`
- `VALIDATION.md`
- `data/alarm.js`
- `data/index.html`
- `docs/AUDIT.md`
- `docs/CHANGES.md`
- `docs/PROFESSIONAL_TESTS.md`
- `remote/.env.example`
- `remote/README.md`
- `remote/api/account.mjs`
- `remote/api/device.mjs`
- `remote/api/maintenance.mjs`
- `remote/api/webhook.mjs`
- `remote/lib/service.mjs`
- `remote/package.json`
- `remote/public/app.css`
- `remote/public/app.js`
- `remote/public/index.html`
- `remote/schema.sql`
- `remote/test/service.test.mjs`
- `remote/vercel.json`
- `src/AppOptions.h`
- `src/Camera.cpp`
- `src/Cloud.cpp`
- `src/Cloud.h`
- `src/Engine.cpp`
- `src/Engine.h`
- `src/Events.cpp`
- `src/Events.h`
- `src/Light.cpp`
- `src/Light.h`
- `src/Motion.cpp`
- `src/Motion.h`
- `src/MotionCore.h`
- `src/Ui.h`
- `src/Web.cpp`
- `tools/embed_ui.py`
- `tools/test-alarm.cjs`
- `tools/test-ui.cjs`
- `tools/test_core.cpp`

Firmware dependencies remain pinned; HTTPClient and WiFiClientSecure are bundled Arduino libraries. The remote Node service has no third-party runtime package dependency, and requires managed Supabase/Meta services only when enabled.
