# Authentication and password recovery

This is an in-place upgrade of the existing `remote/` deployment. It uses Supabase Auth through its REST API, with no browser auth SDK, no new database tables, no recreated account and no change to recipient encryption keys. Existing Supabase user IDs and camera ownership remain unchanged. Meta verification is not required for login/recovery.

## Recover the existing administrator account

1. The permanent customer website is https://vision.robetix.com. Domain/DNS/HTTPS are verified; the passwordless upgrade remains on Preview until production release is authorized.
2. Supabase Site URL is https://vision.robetix.com. The operator saved exact `/auth/callback` and `/reset-password` redirect URLs for that origin and the designated Preview branch. These values were verified with provider-generated links without sending emails. Retain existing redirect entries and do not enable public signup.
3. `remote/service-origins.json` centralizes public origins. Production uses the permanent origin; Preview uses its request origin. Leave `AUTH_APP_ORIGIN` unset in Preview. See [Permanent site and passwordless access](../docs/PERMANENT_SITE.md) for the PKCE email-link flow, trusted devices, revocation and local provisioning. Existing accounts/passwords remain usable through Use a password instead.
4. Open the deployed app, press **Sign in → Forgot Password?**, enter the existing registered email and submit. If it is registered and delivery is available, Supabase sends its recovery email. The response is deliberately the same for an unknown address; it does not promise an email was delivered. Check spam and provider limits. This action neither reveals nor sends the old password.
5. Follow the link, press **Continue**, enter/confirm a new password and save. Use 12–128 characters with a letter and number; common password prefixes are rejected and Supabase may impose additional rules. Sign in with the new password. The API updates the authenticated existing user, then requests Supabase global session sign-out. It does not delete/recreate the user or change camera/recipient records. A provider revocation failure produces an explicit warning rather than claiming all other devices were signed out.

The browser helper is unavailable; the operator saved the Supabase settings and API link generation verified their effective values. The original public deployment, signup-disabled setting, email provider enablement and presence of the existing device/recipient/delivery tables were checked read-only. No real recovery email or actual account password change was performed during automated tests.

## Email link formats

The existing Supabase default **ConfirmationURL** recovery email is supported. Supabase verifies the emailed proof and redirects with a recovery access token in the fragment. The application immediately removes the fragment, keeps the proof only in memory and waits for Continue. The backend validates that exact token with Supabase before inspecting its claims. It requires the correct issuer, user ID, authenticated role, future expiry and a **fresh recovery authentication method** (within 15 minutes). An ordinary login token or a client-supplied `type=recovery` flag is insufficient.

The preferred email template avoids passing a reusable access token through the browser URL. Under **Authentication → Email Templates → Reset Password**, use:

```html
<h2>Reset your Robetix Vision password</h2>
<p><a href="{{ .RedirectTo }}#token_hash={{ .TokenHash }}&amp;type=recovery">Reset password</a></p>
```

The frontend posts that one-time hash to the server, which calls Supabase `/auth/v1/verify` with `type=recovery` and creates a separate, encrypted, 15-minute HttpOnly recovery cookie. The recovery cookie cannot authorize private camera APIs. Reset requires that cookie plus CSRF protection. Page reload can resume this short recovery session. Expired/invalid links give a clear new-link action. No GET page load changes a password; Continue avoids consuming one-time hashes through ordinary email-link prefetching.

Keep email link tracking disabled if your mail provider rewrites or consumes authentication links. Supabase's [password guide](https://supabase.com/docs/guides/auth/passwords) and [JWT authentication-method reference](https://supabase.com/docs/guides/auth/jwt-fields) describe these flows. Custom access-token hooks must retain the recovery AMR claim for the legacy-fragment path; the one-time-hash path is preferred if your claims are customized.

## Email delivery settings

As checked on 2026-10-28, Supabase's [built-in email service](https://supabase.com/docs/guides/auth/auth-smtp) is restricted to authorized team addresses and has tight limits. Its [rate-limit documentation](https://supabase.com/docs/guides/auth/rate-limits) lists two project emails/hour for built-in delivery. Configure **Authentication → Email → SMTP Settings** with your own verified sender/SMTP service for reliable production recovery; dashboard labels may appear under Authentication Configuration. Check sender/domain verification and provider logs. Do not put SMTP passwords in this repository or browser. This implementation does not select/pay for an email service or fabricate an SMTP configuration.

Recovery has additional server limits: five requests/IP/hour and three per email/hour (hashed rate keys), plus ten proof-verification attempts/IP per ten minutes. Unknown-account and upstream email failures have the same public response and a minimum response floor; this is not a guarantee of perfectly equal Internet timing. Check Supabase's own abuse controls and SMTP logs when mail does not arrive. No raw email/password/token is written to application logs.

## Remember This Device and security boundaries

Normal browser login returns only session status, expiry and a CSRF nonce. Access/rotating refresh tokens are held in an **AES-256-GCM encrypted, HttpOnly, Secure, SameSite=Lax, host-only cookie**. The encryption key is derived with a separate HKDF purpose from the existing server-only Supabase service-role secret; this does not change or expose that secret or the recipient encryption key. The cookie is scoped to its app origin and has an absolute lifetime. Rotating the service-role key invalidates these browser cookies.

- Remember selected: persistent cookie capped at 30 days, also subject to Supabase's own revocation/session policy.
- Remember unselected: browser-session cookie and 12-hour server cap. Some browsers restore session cookies when restoring a previous browser session; this option is not a promise of erasure immediately after closing a window.
- Short-lived provider access tokens refresh near expiry through Supabase. The absolute app lifetime is not extended by refresh. Provider token/session policies can end it sooner. Browser Web Locks coordinate refresh among tabs where supported; Supabase's supported refresh reuse interval handles overlapping requests. No in-memory server process must survive a Vercel function restart.
- Each private request validates the cookie and checks the user with Supabase. JSON mutations require exact same Origin and the CSRF nonce. Existing explicit bearer clients remain compatible; the new browser does not retain access/refresh tokens in localStorage. Camera device credentials remain separate from browser sessions.
- Logout clears local cookies/private UI and requests **local** Supabase session revocation. Other tabs clear their UI through BroadcastChannel. Independent devices remain signed in. If provider revocation is temporarily unavailable, local cookies are still removed; it is not a claim that every previously issued bearer token was revoked instantly.
- Supabase access JWTs are not instantly unissued when refresh sessions are revoked. Existing bearer copies may remain valid until expiry; invalid/expired tokens and failed refresh require login. Configure an appropriately short JWT lifetime in Supabase (typically its one-hour default or a reviewed shorter value). This release does not add a per-request database session-revocation registry or MFA UI.
- Temporary auth/network errors preserve the local session and offer retry. Invalid/expired sessions clear private camera/recipient UI. Startup checks the session before showing the welcome/login/private screens, preventing a login-screen flash and any private-data fetch before authentication succeeds.
- Sign-out hides and clears private fields immediately, before waiting for the network. If the browser cannot reach the service, a non-secret logout-pending flag blocks automatic restoration on the next visit until server cookie cleanup can finish. Finish signing out retries that operation; it does not store a credential in localStorage.

The public health endpoint is version 3 and includes Vercel's deployed commit SHA when supplied. It exposes no provider keys or private camera/recipient information. Browser cookie credentials are never returned by the session endpoint. Ordinary users manage recipients through the existing ownership-protected API, which now supports these cookies plus CSRF checks.

## Tests and limits

```text
npm --prefix remote run build
npm --prefix remote test
node tools/test-auth-ui.mjs
node tools/test-remote-ui.cjs
node tools/test-recipients-ui.cjs
node tools/test-ui.cjs
node tools/test-i18n.cjs
node tools/test-recipients-sql.mjs
node tools/check-secrets.mjs --history
```

The 12 auth unit suites and existing 17 service suites use mocked Supabase/Meta responses. The auth browser test executes the actual server handlers with real browser Secure/HttpOnly cookies, and a mocked Supabase provider; it checks saved-browser-state restoration rather than claiming an OS-level phone/browser restart experiment. It also checks refresh, multi-tab logout, outages, reset errors, existing-user password updates, English/Urdu and layouts. Browser tests use Playwright/installed Edge locally and Chromium in CI. No physical camera or real email delivery is established by them.

## Wi-Fi clarification

STA connects the **camera** to a 2.4 GHz router. The firmware also keeps its recovery AP available. Connecting a phone to `ROBETIX-CAM`/the configured camera hotspot does **not** route Internet through the camera. A phone's “No Internet” warning on that AP is expected. Join the router's Wi-Fi for phone Internet and open `http://robetix.local` or the camera's Router IP. If Settings reports Router not connected, check the saved router name/password and 2.4 GHz availability using the local recovery page at `http://192.168.4.1`.

The misleading STA save instruction previously told users to reconnect to the camera hotspot. It now directs them to the router and explains recovery/no-Internet behavior in both languages. Network/capture C++ logic is unchanged. Only this local help-text update needs a firmware upload to appear on the ESP32; remote authentication and GitHub/Vercel updates do not require reflashing. No Internet bridge/NAT or OTA was added. The actual device's router association has not been measured here.

## Local release evidence (2026-10-28)

All **29 API suites** passed (12 authentication/recovery/session and 17 existing service suites). Actual-handler/browser-cookie auth checks, existing hosted recipient/remote UI checks, local camera UI checks, English/Urdu coverage and the PostgreSQL/WASM migration/permission regression passed. These provider responses are mocked; they do not establish actual email delivery, account recovery or Meta delivery.

PlatformIO Core 6.2.0 release compile passed: **71,348 / 327,680 bytes static RAM (21.8%)**, **1,414,617 / 3,145,728 bytes flash (45.0%)**. No firmware upload was attempted. The source checkpoint remains available. Git secret checks passed against tracked files, staged changes, known local credential values and referenced history; `remote/.env.local` is ignored. CI/deployment success must be checked separately for the pushed commit rather than inferred from these local results.
