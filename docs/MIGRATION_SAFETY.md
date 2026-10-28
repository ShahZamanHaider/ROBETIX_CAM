# Migration 003: safety review and backup status

Reviewed 2026-10-28. **No production SQL was executed during this review.** Keep camera setup activation unchanged until a full backup has been verified. A read-only OpenAPI check found `display_name`, `rv_hardware_claims`, `rv_claim_pair_v2` and `rv_bind_hardware` already present in the Supabase project configured in ignored `remote/.env.local`. This establishes their presence, not their exact SQL definitions or installation time. Compare actual function definitions before deciding whether to rerun the revised migration.

## What applying the migration does

- Adds a nullable display-name column; existing names/configuration/owner IDs/credentials are preserved.
- Creates a private hardware-claim table. Its foreign key uses NO ACTION, not cascading deletion: removing a referenced device is blocked until its hardware association is deliberately handled. Ordinary Disconnect disables the device and preserves its history.
- Creates/replaces only the two camera-setup functions and their service-role permissions. Existing authentication, event, recipient, delivery, cooldown and legacy-pairing functions remain unchanged.
- Applying SQL does not execute the pairing/binding functions or consume pairing codes. Later successful pairing consumes only its matching one-use code. Transfer requires prior disconnection and creates a new device identity; old events/recipients remain attached to the old identity.
- Does not modify Auth accounts/passwords, storage buckets or image bytes. No DROP, TRUNCATE, bulk data rewrite or existing-column conversion is introduced.

## Hardening after review

The transaction now fails after a five-second lock wait rather than blocking camera writes indefinitely; individual statements have a 60-second limit. It checks recipient migration prerequisites and rejects an incompatible pre-existing display-name type rather than silently accepting it. Function table/type references are explicitly schema-qualified and temporary objects are searched last. Hardware binding rechecks the enabled device under a row lock, with the same hardware-then-device lock order as pairing. Deployment still briefly needs an exclusive table lock; run during a quiet period and retry later on lock timeout. Do not increase timeouts blindly.

`IF NOT EXISTS` is not a comprehensive schema-drift validator. Existing table constraints, policies, ownership, function definitions and the production version must be reviewed through SQL/dashboard access; the REST service key cannot provide that audit. These functions are SECURITY DEFINER: only service_role should execute them. Server credentials must remain private. Real multi-connection contention still needs a PostgreSQL staging test; the local PostgreSQL/WASM tests do not establish production lock timing.

## Tests

`node tools/test-migration-safety.mjs` executes the real migration over populated PostgreSQL/WASM fixtures. Existing account IDs, device configuration and hashes, events/photo references, encrypted recipients, deliveries, pairing codes, rates, bucket metadata and legacy function bodies/ACLs compare equal before and after. Reapplying preserves existing aliases and claims. Injected failure at the end rolls back the new table, column and functions. Incompatible column type aborts early. Temporary-table shadowing is blocked; revoked devices cannot bind; anon/authenticated cannot read claims or execute the new RPCs.

`node tools/test-camera-setup-sql.mjs` additionally covers five cameras, ownership/transfer and repeatability. `node tools/test-backup-encryption.mjs` checks ciphertext/key/tag tampering rejection.

## Backup created now: supplemental application export

The read-only export is in `C:\Users\shahz\AppData\Local\RobetixVision\Backups\2026-10-28T16-14-28-048Z-A7IOE7`, outside the repository and OneDrive project directory. Finished **2026-10-28 21:14 Pakistan time**. Ciphertext SHA256: `cad7d6da2b3549b8d234f2982620c2705dc99ad63e4ea107cd1d57c64cfab326`.

AES-256-GCM encrypts all contents; Windows DPAPI protects the random key. Key wrapping and full decrypt/readback/count verification succeeded. Do not publish `key.dpapi` or decrypted contents. Recovery requires this Windows account/profile; this is not yet a portable off-site disaster-recovery copy. The encrypted payload includes the existing recipient encryption key needed to recover encrypted contact fields; no service-role or Meta API credential was included. It also contains schema source files, which are repository definitions rather than a dump of the live schema.

Exported row counts: devices 0, pair codes 1, events 0, rates 3, recipients 0, deliveries 0, hardware claims 0. These counts describe the configured project's application tables at export time, not the ESP32 SD card or Auth users. There are no camera/event/contact rows in this export. Pair codes are short-lived and should normally be regenerated rather than restored.

Recheck without revealing rows:

```powershell
node tools/verify-app-backup.mjs 'C:\Users\shahz\AppData\Local\RobetixVision\Backups\2026-10-28T16-14-28-048Z-A7IOE7'
```

**This is not a full database backup.** REST reads are not a transactionally consistent snapshot. The export excludes Auth accounts/password hashes, actual database roles/live functions/policies and Storage object bytes. It cannot restore a lost Supabase project or existing logins by itself.

## Full backup still required

No Supabase management token, SQL connection/password, authenticated dashboard tool, pg_dump or Supabase CLI was available. Windows dashboard automation failed with `failed to write kernel assets ... path specified (os error 3)`. Therefore platform backup availability, its age and a full database restore have not been verified.

Provide a database connection securely as `SUPABASE_DB_URL` in ignored `remote/.env.local`, or use the signed-in Supabase dashboard Database → Backups to check/download the latest restorable backup. Do not paste passwords into chat, replace API keys, rotate the contact encryption key or enable a paid backup plan without reviewing its cost. With SQL access, take a consistent logical dump including Auth and schema, inspect the archive and restore into an isolated matching-version test database before relying on it. Record backup time/project/version/checksum, preserve recipient encryption configuration separately, and keep a recoverable encrypted off-site copy. Restore is a separate operation; never test by overwriting production.

Supabase database backups exclude actual Storage objects. Export any required media separately. Official instructions: [Database backups](https://supabase.com/docs/guides/platform/backups), [logical backup scope](https://supabase.com/docs/guides/self-hosting/restore-from-platform).
