# Agent handoff: connect Google later

The code is implemented, but preparing source does **not** connect a Google
account or install privileged support on a frame. Resume this guide when the
owner asks to activate Google. Keep the existing iCloud account, Wi-Fi,
household profile, AI budget/spending ledger, and update signing key intact.

## Before doing anything

1. Read repository/workspace instructions, this directory's [setup guide](README.md),
   [account boundaries](../account/README.md), and [signed update guide](../update/README.md).
2. Check the working tree and existing build/test evidence. Do not discard unrelated
   changes or reuse binaries built from a different source revision.
3. Confirm the owner wants the Google account connected **alongside iCloud**.
   Only one Google account is supported, with up to 16 selected calendars.
4. Ask for the local path of the downloaded **Desktop app** client JSON, not its
   contents. Keep it outside tracked source or inside an ignored private directory.
   Do not print its client secret, access token, refresh token, or OAuth callback URL.
5. The owner completes Google browser sign-in/consent. Do not use their primary
   password, a service account with domain-wide delegation, the deprecated
   copy/paste authorization-code flow, or the limited-input device flow (Calendar
   scopes are not supported there). ChatGPT's Google connector does not provision
   the native frame's OAuth credential.

## Google Cloud preparation

Use the owner-owned Google Cloud project and enable **Google Calendar API**.
Configure Google Auth Platform Branding/Audience/Data Access, with only
`https://www.googleapis.com/auth/calendar.readonly`, then create a **Desktop app**
OAuth client and download its JSON. Use Internal audience only if the project
belongs to the Workspace organization and the desired account is in it. Otherwise
add the owner as an External test user initially. For daily use, address Testing's
seven-day refresh-token expiry with the appropriate production/internal audience.
A Workspace admin may need to allow the client in API controls. The human setup
steps and official documentation links are in [README.md](README.md).

## One-time privileged support on the SD frame

The originally deployed Wi-Fi updater installs only the two native binaries.
After the [migration bootstrap](../update/README.md#signed-credential-support-migrations),
signed releases can add encrypted credential declarations to the calendar service.
They still cannot install the root credential importer/credential writer or upgrade
its own privileged code. A trusted USB maintenance connection is required once
for these additions. This is separate from future ordinary Wi-Fi app updates.

- Verify the attached device is the expected PF1007L SD Linux gadget using the
  existing hardware-bound USB tooling. Do not choose an unrelated serial device
  or rely on a previous disk identifier/IP/MAC.
- Verify `/` is `/dev/mmcblk0p2`, the card type is `SD`, and no `MMC` internal
  device is exposed. Use the existing maintenance account; do not enable SSH,
  an inbound network service, or new sudo rights for the dashboard.
- Stage these **source files only**, together in a private installation directory:
  `google_bundle.py`, `import-google.py`, `install-google.sh` from this directory,
  plus `credential-vault.py` and `porthole-calendar.service` from `../account/`.
  Include `SHA256SUMS` for these exact source bytes if packaging a bundle.
  No client JSON, credential, private household data, or signing key belongs in it.
- Transfer with the already established trusted USB path and verify the bundle's
  checksum. Extract only the expected files. On the frame, run
  `sudo bash install-google.sh` from that staging directory.
- Expect `GOOGLE_CREDENTIAL_SUPPORT_INSTALLED` (or
  `GOOGLE_CREDENTIAL_SUPPORT_ALREADY_INSTALLED`). Verify installed files match
  source, root ownership/modes, and active credentials/calendar services using
  `systemctl --no-pager`. The installer preserves the old vault/unit and restores
  them if the install fails. If previous evidence exists with differing files,
  inspect it before retrying; do not delete the backup to bypass the guard.
- Do not decrypt production credentials for inspection. Do not write internal
  storage, change the kernel, or install new packages for this integration.

## Native app and sign-in

1. Build the native ARMv7 app/backend from the intended source and run all CTest
   suites. Run `python3 linux/google/setup-tests.py`,
   `python3 linux/account/vault-tests.py`, and `python3 linux/update/update-tests.py`.
2. Use the existing private release signing key. Never generate a replacement key
   for an enrolled frame. Publish a fresh signed version/sequence following the
   update guide, then let the frame's Wi-Fi updater install it. Alternatively,
   confirm an already installed release contains Google support. Pushing source
   alone does not install software.
3. From an interactive Mac terminal, with the SD Linux frame attached over USB:

   ```sh
   python3 linux/google/connect-google.py --client /private/path/to/client.json
   ```

   This opens the owner's browser and a temporary loopback callback listener,
   validates random state and S256 PKCE, obtains an offline refresh credential,
   and sends it only after the receiver's no-echo handshake. No additional Python
   packages or Google password entry on the frame are needed.
4. Confirm the helper's fixed success message. It means encrypted import and
   backend restart succeeded, **not** that calendar retrieval has already passed.
5. In Settings → Calendars → Google, confirm Google connected and select the
   intended calendars. Initial selection is the primary calendar; other shared
   or subscribed readable calendars are optional. Leave iCloud's selection intact.

## Verification without leaking family data

Inspect only bounded metadata/flags from the calendar service, using the allowed
GUI identity. Never print the full status response: it contains private profile,
calendar names, events, and briefing. Never print decrypted service credentials.

Check that Google becomes authenticated, its last-checked timestamp advances,
selected calendar count is expected, iCloud is still authenticated, and the
merged calendar/briefing health has no error. Ask the owner to confirm a known
Google event appears on the correct local date next to their iCloud events.
Check both timed and all-day events if available. Tests already cover DST,
exclusive all-day end dates, pagination, access-token renewal/revocation,
redirect restrictions, sorted merging, and withholding partial-source briefings.

Restart the calendar service and app, then confirm selections and remembered
connections restore. A complete power-cycle check can follow when convenient.
Compare private settings semantically and the spending ledger conservatively;
do not assume a default AI budget or reset a ledger. Verify settings/touch in
portrait and landscape without publishing a real calendar screenshot.

For revoked/expired sign-in, run the same Mac browser helper again. Temporary
network errors retry every minute. Forget Google removes only the frame's local
Google credential/selection and restarts the backend; the owner can revoke the
Google grant separately in Google Account connections. Google's API scopes are
read-only but cover the account's accessible calendars; Porthole itself retrieves
only selected calendars.

Report separately what is proven by automated tests, installed on the frame,
and verified with the owner's live account. If USB/client JSON/browser consent
is unavailable, leave the prepared work intact and state the exact next action.
