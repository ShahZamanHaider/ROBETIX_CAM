# Files changed in the professional upgrade

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
