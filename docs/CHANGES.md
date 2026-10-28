# Files changed in the professional upgrade

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
