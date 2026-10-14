# Persistent iCloud sign-in on SD Linux

The dashboard runs as `porthole`, with no sudo access and no permission to read the host encryption key or contact the credential writer directly. The read-only CalDAV connector runs as the separate `porthole-calendar` user. The two communicate over a local Unix socket whose directory and kernel peer-UID checks restrict access to the dashboard identity. The calendar service only sends calendar metadata, status and events back to the UI; there is no password-read operation.

A root-owned helper accepts bounded save, forget, clear and storage-status messages only from the calendar-service UID. It has a fixed credential path and no caller-controlled command or filename. It encrypts account email, the dedicated app-specific password and selected calendars with `systemd-creds --with-key=host`, passes plaintext through standard input, and atomically replaces the encrypted file after syncing it. No plaintext credential file, secret command argument, environment variable or request log is created.

`porthole-calendar.service` receives the decrypted credential through systemd's service credential directory. The encrypted file is `/etc/credstore.encrypted/porthole-icloud`; the host key is under `/var/lib/systemd/`. Both are private to root. Core dumps are disabled for the calendar service and dashboard. Calendar discovery must succeed and the user must select readable calendars before the account is saved. A remembered account reconnects and restores those selections when the service/device restarts; an app restart reconnects to the existing service. Network failures retry asynchronously. A rejected credential stops automatic authentication attempts and displays a request to replace the password.

The frame exposes no TPM device. With automatic sign-in, the key and encrypted credential reside on the same removable SD card. This protects ordinary process/file access but does not prevent recovery by someone with the complete card or root access. Keep the actual credential, host key, Wi-Fi profiles and live calendar data out of source control, diagnostics and shared backups. Forget removes the saved credential and restarts the calendar service to discard its loaded plaintext credential; it does not revoke the app-specific password at Apple or promise forensic erasure of flash storage.

The dashboard's existing USB maintenance account remains separate from the app identity. Internal Android/eMMC and the pinned kernel are unchanged by this installer.

## Verification

Native tests cover destination restrictions, DAV/recurrence/date parsing, changed/unchanged calendar markers, unsupported-marker fallback, rejected credentials, both Settings orientations, account controls, password visibility, full-row checkbox touch targets, keyboard boundaries and input responsiveness with a slow fake iwd daemon. Python tests cover the credential helper's schema and destination restrictions.

On the physical frame, a separate synthetic credential fixture passed authenticated encryption, tamper/name rejection, restricted delivery, two service starts and a clean reboot. Save and removal also passed through an isolated copy of the production writer under its exact sandbox. Production sockets rejected unapproved peers, the dashboard could not read the host key or reach the credential writer, and a primary-password fixture was rejected without contacting Apple or creating a saved account. The empty-account Forget action restarted the calendar service without disturbing Wi-Fi. Reboot results are recorded in the bring-up notes. None of these synthetic checks substitutes for the owner's final sign-in with the actual account.

`install-account.sh` verifies SD root, absence of eMMC and a sealed bundle manifest, preserves the previous app and compositor/service configurations, and restores them if installation fails. Retrying a rolled-back installation requires the restored files to match the original backup. Runtime packages and kernel/modules are not upgraded.

## OpenAI credential

The calendar service also loads the separate encrypted `porthole-openai`
credential for background briefings. A fixed-purpose root receiver provisions
it through a hidden Mac-to-frame USB prompt. The GUI has no credential-read API.
The credential writer accepts a fixed `forgetOpenai` operation from the calendar
service only; it cannot choose an arbitrary file. Spending state and cached
briefings live in a service-private StateDirectory. See [AI setup](../ai/README.md).
