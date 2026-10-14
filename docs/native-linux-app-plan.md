# Native Linux Porthole — implementation record

The owner requested that, after the browser hardware test, Porthole be rebuilt as a native Linux app with authenticated access to their iCloud calendar. The browser kiosk is a temporary validation path. Preserve the current Today, Tomorrow, and Calendar design and the configured weather location weather requirements when implementing the native UI.

The initial plan and checkpoints below are historical. The current native app, saved direct iCloud sign-in, live weather, Wi-Fi and bounded AI briefings are implemented and verified. Start with the [main setup guide](../README.md).

The target is ARMv7 with approximately 1 GiB RAM and a 1280×800 landscape display. Linux SD boot, visible panel output, display autostart, and physical touchscreen events are verified. Browser/compositor rendering and touch alignment are the active test. The pinned kernel and matched modules must remain compatible.

Authentication must use a supported iCloud access method. Apple documents app-specific passwords for third-party calendar applications and separate Apple Account authorization for supported apps; do not assume that general Sign in with Apple grants calendar access or that the custom app qualifies for the latter flow. See [Apple's authentication guidance](https://support.apple.com/en-ca/102654). The owner selected a direct iCloud connection on the frame, using a dedicated app-specific password. Never request their primary Apple Account password.

Credentials must be entered through a secure local flow and kept out of the frontend, logs, source code, and published artifacts. Select and verify the native secret-storage mechanism before collecting a credential. Read-only calendar discovery and display are the initial integration; remote calendar edits or invitations are outside the requested dashboard scope. Keep synthetic test data clearly labelled until real authentication succeeds.

The authenticated connector will run on the frame, as the owner requested. The native UI toolkit remains an implementation decision against the verified ARMv7 hardware and credential-storage requirements. The current offline browser test does not enable Wi-Fi or connect to iCloud; network setup remains a separate checkpoint.


## Native prototype checkpoint — 2026-10-13

Original C++/Qt Widgets source is now in [linux/native](../linux/native/README.md). It built for ARMv7 against Qt 6.12 and libical 4.0.6; six calendar/authentication fixture cases passed. Landscape and portrait renderings were visually checked, and the real frame screenshot confirms a fullscreen native window. Matching, signature-verified runtime packages were installed on SD without upgrading the kernel or existing package baseline. The previous browser configuration is retained privately as a rollback option.

The frame-side executable verifies available TLS support and the configured local timezone. Runtime checks retain kernel 7.2.10, SD root, loopback-only networking, and active display/compositor/orientation services. The browser process has exited. Total system memory use was approximately 140 MiB with no swap in use at the native checkpoint. The frame's clock was synchronized once from the Mac over USB; reliable clock synchronization after later power cycles remains part of network setup.

The initial authentication flow uses a native masked input with touch keyboard and a dedicated app-specific password held only in session memory. It lists calendars before reading selected calendars. No credential, calendar-data cache, or live iCloud request has been stored or made. Reconnect after restarting. Persistent encrypted credential storage remains a separate implementation choice.

Live iCloud compatibility and weather still require network setup and the owner's on-device sign-in. The parser requests server-expanded recurrences and explicitly rejects responses that leave recurrence expansion undone. Fixture success does not establish compatibility with the actual account. Native startup then passed a warm reboot: all four display/compositor/sensor/orientation services were active, the native fullscreen window returned, fixed touch calibration remained, and no units failed. The RTC initially read 1970; it was updated from the Mac-synchronized system clock and retained the correct time through that warm reboot. Retention after complete power loss is unverified. Native tab/touch/portrait interaction still awaits the owner’s physical check, distinct from the already verified browser hardware test.


## Approved persistent-account implementation — 2026-10-14

The owner approved the Show password toggle, remembered direct iCloud sign-in, restored calendar selections, account management controls, one-minute calendar change checks, independent 15-minute weather refresh and a Wi-Fi input refinement. The implementation separates the restricted GUI, restricted read-only calendar connector and fixed-purpose root credential writer. It uses systemd host-key credential encryption on SD, with the previously discussed physical-card/root-access limit. The installed GUI no longer has blanket sudo access. See [account implementation](../linux/account/README.md) for details and tests. Actual account storage requires one final on-frame sign-in; earlier versions held credentials only in their process memory.
