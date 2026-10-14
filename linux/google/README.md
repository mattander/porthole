# Google Workspace / Google Calendar

Google calendars appear alongside iCloud in the native SD Linux dashboard and
family briefing. The frame calls Google directly, checks every minute, expands
recurrences on the server, and handles all-day dates with an exclusive end date.
This integration reads calendars and events. It does not access Gmail, Drive,
contacts, or create/change events. Google Calendar push requires a public HTTPS
webhook, so this local frame polls instead.

## 1. Create your Google OAuth client

Open [Google Cloud Console](https://console.cloud.google.com/).

1. Create/select a project for your personal Porthole installation.
2. In **APIs & Services → Library**, enable **Google Calendar API**.
3. Open **Google Auth Platform → Branding**. Name the application Porthole and
   supply your own support/contact email.
4. In **Audience**, use **Internal** if your project belongs to the Workspace
   organization and only its users will sign in. Otherwise choose **External**,
   add your account under test users, and use Testing while checking setup.
5. In **Data Access**, add exactly
   `https://www.googleapis.com/auth/calendar.readonly`.
6. In **Clients**, create an OAuth client of type **Desktop app**. Download its
   JSON. Keep it outside the public repository (or in your ignored `local/`
   directory). Do not paste its contents into chat or commit it.

A Workspace administrator may need to allow the client ID in
**Security → Access and data control → API controls → App access control**.
Internal audience only works for accounts in that organization. The application
requests a read-only scope, but this scope can read all calendars the account
can access; the frame restricts retrieval/display to your selection.

External apps left in **Testing** receive refresh tokens that expire after
seven days with this Calendar scope. For durable daily use, configure an
appropriate production/internal audience. External users can see an unverified-app warning and user limits apply until
the required verification is completed. Follow Google's consent-screen
requirements for your audience.

References: [Desktop OAuth and PKCE](https://developers.google.com/identity/protocols/oauth2/native-app),
[token expiration](https://developers.google.com/identity/protocols/oauth2#expiration),
[verification requirements](https://support.google.com/cloud/answer/13464321),
[Workspace app access controls](https://support.google.com/a/answer/7281227).

## 2. Install the once-only credential support

An existing frame needs the trusted Google importer, fixed credential-forget
operation, and systemd credential declaration once. This maintenance step runs
on the SD Linux system over the existing USB maintenance route; it does not
write internal storage, install packages, or change the kernel. Stage these
files together in a private installation directory:

- `google_bundle.py`, `import-google.py`, `install-google.sh` from this directory
- `credential-vault.py`, `porthole-calendar.service` from `../account/`

Run `sudo bash install-google.sh` from that directory on the frame. Install the
native version with Google support using the existing signed Wi-Fi updater.
These root helpers are intentionally outside the two-binary update archive.
For a later activation, follow the [agent handoff](AGENT-SETUP.md).

## 3. Sign in once through your Mac

Connect exactly one PF1007L frame, booted from SD Linux, over USB. From your Mac
terminal at the repository root:

```sh
python3 linux/google/connect-google.py --client /path/to/downloaded-client.json
```

No extra Python packages are needed. The helper opens your normal browser and
binds a temporary callback server to `127.0.0.1` only, using random OAuth state
and S256 PKCE. Choose the intended Google account and approve read-only Calendar
access. It then sends the refresh credential over the matched USB device only
after the frame disables terminal echo. Tokens are not written to a plaintext
file, terminal log, command argument, environment variable, or GitHub release.

The frame encrypts the credential with `systemd-creds`, restarts its calendar
backend, and initially selects the primary Google calendar. Choose additional
calendars in **Settings → Calendars → Google → Choose Google calendars**.
Choose up to 16 Google calendars, independently of your iCloud selection.
Selections and remembered sign-in survive app/frame restarts. iCloud remains
connected. Subsequent refreshes work directly over Wi-Fi without your Mac.

## Disconnect and storage

**Forget Google** removes the frame's encrypted Google credential, selection,
and loaded session by restarting the backend. It preserves iCloud, Wi-Fi,
household settings, and the AI spending ledger. To revoke permission at Google,
remove the application from [your Google Account connections](https://myaccount.google.com/connections).
Forgetting a frame's local credential does not itself revoke the Google grant.

The root-private encrypted credential is
`/etc/credstore.encrypted/porthole-google`. The backend alone receives its
plaintext through systemd service credentials. Selection metadata lives in
`/var/lib/porthole-calendar/google-selection.json` with mode 0600. No token-read
IPC operation exists. Google API requests use a Bearer header, fixed HTTPS
hosts, no redirects, bounded responses/pages, and no raw response logs.
Revoked/expired refresh credentials stop automatic sign-in attempts and show a
reconnect message. Temporary network errors retry without changing saved data.

Host-key encryption protects ordinary application access. Someone with the
complete SD card or root access can recover stored credentials, as with iCloud.
One Google account per frame is supported in this version; shared/subscribed
calendars readable by that account can be selected.

## Checks

```sh
python3 linux/google/setup-tests.py
```

The native `google-oauth-calendar-boundaries` suite covers token refresh/revocation,
read-only requests, recurrence query parameters, pagination, credential parsing,
source identity, date validation, all-day exclusive end dates, and retry after an
expired access token. UI tests cover controls in landscape and portrait. A live
Google account test also requires the owner's OAuth client and browser consent.
