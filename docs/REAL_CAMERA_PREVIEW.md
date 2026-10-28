# Real ESP32-CAM: isolated Preview firmware

The hardware test is pinned to the tested immutable deployment and Git commit recorded in ignored `.pio/preview-access.json`. The latest verified website address is reported after deployment; do not substitute the moving branch alias for a hardware build.

One deployment-level Vercel protection exception allows requests to that Preview without interactive Vercel SSO. Its branch alias also resolves to that same deployment until another deployment replaces it. This is not a project-wide protection change. Robetix account login, CSRF checks, ownership checks, one-use pairing codes, hashed device credentials and short-lived relay tickets remain enforced. Production remains on its existing branch/deployment. This public test deployment uses the same configured Supabase project, so preserve existing users/settings; do not run reset/schema scripts to prepare a camera.

No Vercel bypass credential is compiled, stored in NVS or sent by the camera. The only build override is the **public HTTPS origin**. The exception does not expire automatically; remove it after testing using `patch_url_protection_bypass` for `DEPLOYMENT_ID_FROM_PRIVATE_CONFIG` with `override: {action: "revoke", scope: "alias-protection-override"}`. Removing it restores the project's protection for this deployment. Do not disable project-wide SSO. Future deployments do not inherit this deployment-specific exception; keep the hardware on the pinned URL until an explicitly tested replacement is chosen.

## Build configuration

Normal builds still default to `esp32cam`, which uses `https://robetixvision.vercel.app`. The separate `esp32cam_preview` environment uses ignored `.pio/preview-access.json`. The pre-build script accepts exactly these public fields; no cookies/tokens/query parameters are permitted:

```json
{
  "mode": "single-preview-exception",
  "service_origin": "https://robetixvision-example-robetix-s-projects.vercel.app",
  "deployment_id": "DEPLOYMENT_ID_FROM_PRIVATE_CONFIG",
  "git_commit": "FULL_40_CHARACTER_GIT_COMMIT"
}
```

Keep that file and `.pio/generated/preview/PreviewService.h` Git-ignored even though the current values are public metadata. Never replace the origin with a `_vercel_share` URL, cookie or automation bypass secret. The build rejects extra fields and query strings. A clean checkout builds production without needing this private configuration. Generated firmware binaries live under ignored `.pio/build/`.

From the existing PlatformIO project:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e esp32cam_preview
```

The prepared binary is `.pio/build/esp32cam_preview/firmware.bin`. Flash only after identifying the programmer and correct serial port; no upload was performed during preparation. Use `run -e esp32cam_preview -t upload --upload-port COM_PORT`, replacing COM_PORT only with an observed port. Avoid full flash erase: it removes existing Wi-Fi/admin settings, factory setup-card identity and device configuration. Keep the OV2640 native JPEG pipeline and the existing board/pin/partition settings.

## Guided physical setup sequence

The assistant will walk through these one step per reply: identify/connect the USB programmer; detect its serial port; enter upload mode if required by that adapter; upload the prepared Preview environment; return to normal boot; connect to the camera's existing Wi-Fi/setup card; sign in locally and choose router STA mode; open the pinned Preview and sign in; generate Add Camera code; enter it under local Settings → Connect to my account; check sensor/PSRAM/connection status; open Watch Live on a phone using mobile data. Do not enter Meta/Supabase/Vercel credentials on the camera.

Simple pairing uses this build's origin. Existing valid device credentials are retained when a firmware update moves between recognized Robetix Preview/Production origins; unrelated custom services are left unchanged. A fresh code is required for an unpaired camera or an explicitly requested new pairing. Existing camera, network, recording, motion and SD preferences remain stored. If an existing cloud ownership claim blocks pairing, disconnect that camera in its current owner's dashboard first; do not delete users, reset the database or clear the factory namespace.

The local Settings link displays the build's website address automatically. Capture and recording settings remain conservative. Hardware image quality, frame rate, serial programming, power stability, SD coexistence and outside-LAN streaming still require real-device tests. Browser/relay tests with a simulated JPEG sender do not establish ESP32 performance.
