# PF1007L Linux from microSD

## Checkpoint — 2026-10-13

The PF1007L now boots fresh Arch Linux ARM from the separate PORTHOLE microSD with pinned kernel **7.2.10-1-armv7-ARCH**. Display, physical touch, the fullscreen Firefox/Sway dashboard, accelerometer-driven rotation, and saved startup settings have passed physical checks. The owner confirmed upright display and correctly placed taps in all four directions. A subsequent clean reboot verified all display/kiosk/web/orientation services active, the fixed touch calibration retained, and no failed units. Root remains on SD, no eMMC device appears, and networking remains loopback only.

The sections below record the staged investigation and distinguish earlier pending checks from the final runtime checkpoint. The card write review retains the original failed exact-hash check and the subsequently established FAT metadata exception.

This work uses the new approximately 64 GB PORTHOLE microSD. The original approximately 32 GB FEL recovery card remains separate. The original Android backup and the already verified USB-root boot partition remain the fallback; no additional internal eMMC write is part of this Linux test.

## Basis and hardware match

The source is [pangoleen/ormarch-picture-frame at a3d83874](https://github.com/pangoleen/ormarch-picture-frame/tree/a3d83874aabcae655f245973e6408937a591e39f). Its author reports a working PF1007L SD installation with display, touch, GPU, and Wi-Fi, while distinguishing that installation from a newly built image. That report does not establish compatibility with our physical unit.

We compared this unit's original, verified firmware with the candidate configuration:

| Input | Evidence from our unit |
| --- | --- |
| CPU and memory | Allwinner A33 identified through FEL; approximately 1 GiB RAM |
| DRAM configuration | Original boot header: 456 MHz, ZQ 31739, DDR3, ODT enabled |
| Display | Native 800×1280 four-lane MIPI-DSI; panel yhk_zr789_sq9365_101 |
| Panel setup | Initialization commands extracted from the matching source-file symbol in this unit's original LCD module |
| Touch | gsl_m86_hd, I2C address 0x40; extracted firmware matches the candidate's pinned digest |
| Board wiring | Panel power/reset/backlight, touch IRQ/wake, and regulator settings compared with the original FEX |
| Wi-Fi | Loaded stock aic8800 drivers; the candidate's AIC8800 modules built for the pinned kernel, with automatic loading disabled |

These checks justify a staged removable-card test. They do not establish that every driver will work. Private vendor modules, firmware blobs, original configuration dumps, SSH keys, and generated images remain outside Git.

## Image and validation

The 3 GiB MBR image contains U-Boot at the Allwinner SD boot offset, a 256 MiB FAT32 partition named PORTHOLE, and an ext4 root partition. The upstream first-boot service is configured to grow that root partition to the remaining card space. Its physical operation is still unverified here.

The U-Boot build uses v2025.10 (e50b1e8715011def8aff1588081a2649a2c6cd47) with the reviewed PF1007L patches. Our conservative initial build uses a 408 MHz CPU and omits U-Boot PMIC initialization. Initial Linux labels select the powersave governor. The original board's DRAM values are retained. UART pin muxing is disabled because those pins have other board functions.

U-Boot has no extra eMMC slot; its embedded device tree and every selected Linux boot device tree disable the eMMC controller. Its fallback USB storage export is read-only. The root filesystem contains a secondary eMMC read-only rule, whose operation on physical hardware remains untested.

Completed host checks include:

- Image size, partition geometry, embedded U-Boot equality, and SPL header checksum.
- U-Boot configuration guards and read-only USB storage write-refusal code.
- All selected device trees, default minimal boot label, powersave selection, and serial-only USB configuration.
- FAT and ext4 filesystem checks and image digest consistency.
- ARMv7 userspace execution under QEMU user emulation; kernel/module version and symbol compatibility.
- Card-selection guards rejecting internal, incorrect-capacity, non-removable, non-writable, and recovery-card targets.

We did not perform a full-machine QEMU boot of our assembled image. Upstream emulator results are separate evidence. The root filesystem is currently a minimal Linux base; a desktop/browser kiosk has not been provisioned.

## Card write and first boot

The private writer selects the original PORTHOLE volume using its recorded UUID, exact media capacity, reader model, sector size, and external/removable status. It checks the blank filesystem, reads the original prefix twice, and preserves it. After normal unmount it rechecks stable MBR/exFAT boot identity, allowing filesystem metadata that legitimately changes during unmount. It writes only the new card, performs two independent full image readbacks, and normally ejects the card after both pass.

The first attempt stopped before writing any image bytes because it compared mutable filesystem metadata across unmount. Its evidence is preserved. The corrected guard was tested against mutable-metadata and changed-identity cases. The retry wrote the complete image and its first full readback matched exactly. macOS then remounted the FAT boot partition; the second exact-image hash failed. Both subsequent independent full-card comparisons agree: the only difference is two bytes in the FAT32 FSInfo next-free-cluster hint, changing its value from 31931 to 0. The MBR, bootloader/reserved region, FAT allocation tables and file contents, and entire ext4 root partition match the original image. All 980 boot files also passed a separate manifest comparison. This is a reviewed metadata exception, not two exact image-hash matches. The failed verification and comparison evidence remain private and preserved. Normal eject succeeded; no rewrite was necessary.

After the exact write verification or a reviewed equivalent check confirms readiness and normal eject:

1. Fully power off the frame by disconnecting its power and USB cables.
2. Insert the new PORTHOLE Linux microSD into the frame's TF slot, leaving the original recovery card aside.
3. Connect the known working USB-A–C cable directly to the Mac.
4. Inspect the frame's USB enumeration and USB serial console before changing boot stages.

The default label is **min**: SD and USB only. **A dark screen is expected.** USB networking is disabled, Wi-Fi is not automatically loaded, and home-network access has not been configured. The intended gadget is `1d6b:0104`, product `PF1007L Linux`, exposing a serial console. Enumeration on this frame and Mac remains to be tested.

The first runtime checks should establish the actual kernel, SD root device, absence of an eMMC device, CPU frequency, memory, and boot errors. Only after that checkpoint should we advance to the PMIC and staged display/touch configurations. Those stages can be selected on the removable card; Android remains available by powering off and removing it.

If the initial boot fails, power off and inspect the card's boot markers. The upstream boot script is designed to enter read-only SD USB export on the next start after an unconfirmed Linux boot. Its physical recovery behavior is not yet verified. Do not substitute a different firmware image or write internal storage to resolve a failed SD test.

## Dashboard direction

Once display, touch, and a browser are verified, Porthole can use its existing frontend as a Linux kiosk. The owner subsequently requested a native Linux application that connects directly to iCloud using a dedicated app-specific password; weather is for the configured weather location. See the [native app plan](native-linux-app-plan.md). No calendar credentials or live calendar/weather service have been configured yet. The offline Android preview remains a working interim display path.

## First physical Linux boot — verified

The frame enumerated as `1d6b:0104`, product `PF1007L Linux`, and its USB serial console logged in successfully. Runtime checks established kernel `7.2.10-1-armv7-ARCH`, architecture `armv7l`, and an ext4 root filesystem at `/dev/mmcblk0p2`. The sole MMC block disk reports type `SD`; no eMMC block device appears. The root filesystem expanded to approximately 57 GiB usable filesystem capacity on the new card. MemTotal is 1,002,044 kB.

The first-boot service completed, Linux acknowledged the USB boot in its SD marker, and systemd had no failed units. Only the `lo` network interface exists. The powersave governor holds the CPU at 120 MHz in this initial stage. Boot messages include missing CPU device-tree clock-frequency properties and a GPU probe error caused by the minimal tree's absent regulator; the GPU/display path has not yet been enabled or validated.

The next SD boot label was selected as `pmic`, and a clean Linux reboot was issued. PMIC-stage success must be checked separately; display, touch, Wi-Fi, and eMMC remain disabled in that stage.

## PMIC stage — verified

The physical `pmic` stage booted successfully and retained SD root, no eMMC block device, loopback-only networking, and no failed systemd units. Linux identified the AXP223 over RSB. Regulator readback reported CPU/system at 1.2 V and DRAM at 1.5 V, matching the original board configuration. The minimal-stage GPU regulator error disappeared with that GPU node disabled. The display, touch, and Wi-Fi nodes were still disabled during this checkpoint.

The SD was then switched to `full-stage1`, which supplies the full board device tree while blocking automatic display/GPU/Wi-Fi module loads. That stage's success and manual driver bring-up require separate runtime evidence.

## Display stage and automatic startup — verified

The physical `full-stage1` boot retained SD-only block storage, loopback-only networking, and no failed systemd units. Its CPU uses schedutil, reaching the pinned 1.008 GHz maximum. The Silead driver identified chip ID `0x50910000` and created the `silead_ts` input device using the extracted firmware.

Manual, ordered module loads brought up the backlight, the matched panel module, the MIPI-DSI host, and the sun4i display pipeline. Linux reports DSI-1 connected at native 800×1280 and provides `sun4i-drmdrmfb`. The owner confirmed that Linux text is visible on the physical screen.

The tested display sequence is saved as original Porthole code in [linux/](../linux/README.md). Its service was syntax/definition checked, enabled, and tested through a clean reboot. It remained active with the connector and framebuffer present. The boot selection remains `full-stage1`; automatic Wi-Fi and GPU loading remain blocked. The service explicitly loads only the verified display sequence and checks that root is on SD with no eMMC block device before doing so.

A manual Lima probe identified Mali-400 with two pixel processors and created a render node. A missing GPU-regulator message remains, and actual accelerated rendering has not been tested. Lima is deliberately excluded from the automatic display service until the renderer test.

A bounded read-only touchscreen capture recorded zero events. The owner subsequently confirmed missing that capture window; its empty result does not establish a failed controller. The repeated capture, with owner taps and drags, received 500 input events including multitouch tracking and changing X/Y coordinates. Physical touch input is verified. Coordinate alignment with a rotated browser dashboard remains open. A browser/compositor kiosk and live dashboard backend have not been installed or connected.


## Browser rendering, calibrated touch, and rotation — verified

Official, signature-verified Arch Linux ARM packages supplied Firefox 157.0 and Sway 1.12 without replacing the pinned kernel. The browser serves an explicit fictional sample dashboard from `127.0.0.1:8765`; no iCloud credentials or live services are configured. An actual 1280×800 frame screenshot confirms the complete fullscreen UI. The compositor uses the probed Mali render node; this does not establish every Firefox acceleration feature.

The touch controller's axes require the fixed Sway calibration matrix `0 1 0 1 0 0`. Both landscape and portrait tab presses were recorded at the correct UI elements. Calibration stays fixed while Sway maps touch through each output transform.

The original board's I2C1 sensor at address `0x19` returned identity register `0x11` and bound to the installed kernel's `sc7a20` driver. Its default 1 Hz sampling setting caused three-axis reads to take approximately six seconds. Setting the available 25 Hz rate reduced a complete read to approximately 0.26 seconds. The rotation service waits for stable gravity direction and holds the previous orientation when the frame is flat or diagonal. The owner confirmed all four orientations and correctly placed taps.

Display, kiosk, sensor, and orientation startup are enabled on the Linux SD filesystem. A clean reboot retained kernel 7.2.10, SD root, 25 Hz sampling, fixed touch calibration, a running browser, and no failed units. Temporary click diagnostics were removed and the local HTTP response checked. One intermediate reboot failed because removing those diagnostics accidentally truncated the server's startup call; the server was corrected and the successful reboot repeated. No additional eMMC write or network activation occurred.


## Native Settings and Linux Wi-Fi checkpoint — 2026-10-14

The original native app now has a Settings pane with Wi-Fi, iCloud, Display and Data tabs. The main dashboard keeps clock, weather and plans together; connection, appearance and refresh controls are in Settings. Offline visual checks covered light/dark and portrait/landscape layouts, and a private screenshot confirmed the installed real Wi-Fi picker fills the physical 1280×800 display and lists nearby networks. Credential screens are not captured.

The AIC card's actual SDIO identities are `C8A1:C08D` and `C8A1:C18D`. A first driver load after the card had been idle for hours failed in SDIO setup with `-110`. Resetting only the verified Wi-Fi host `1c10000.mmc` cleared this transport failure; firmware initialized and `wlan0` appeared using the existing ABI-matched modules. A PMIC register check showed GPIO0 control `0x01` (output high); the GPIO debug value alone did not indicate its output setting. The underlying timeout cause is still unconfirmed. No module rebuild or internal firmware write was needed.

The guarded host initialization is saved in [linux/network/](../linux/network/). A clean Linux reboot verified active native kiosk, rotation, Wi-Fi, iwd and firewall services, SD root, the `wlan0` adapter, and the same private stable Wi-Fi MAC assigned before iwd starts. SSH is disabled, and mDNS/LLMNR are disabled. The inbound firewall is active. The frame has not joined a network at this checkpoint; network selection/password entry and a live iCloud account check remain the owner's next steps.

The native iwd password agent rejects an unrelated root caller and requires the requested path to match the user's selected network. Saved Wi-Fi credentials are handled by iwd on SD; iCloud credentials remain memory-only in the native app. Joining a network automatically switches the dashboard from clearly marked sample data to live data.


## Keyboard responsiveness follow-up — 2026-10-14

The owner reported missed keyboard taps and paused cursor blinking. Live diagnostics found 828 MiB available RAM, no swap use, no memory pressure or OOM events, CPU about 93–96% idle in the short sample, a 1.008 GHz maximum/current frequency, and temperature about 46°C. Those readings did not support memory exhaustion or thermal throttling.

The native Wi-Fi controller constructed dynamic `QDBusInterface` objects on the UI thread during polling and operations; construction can synchronously introspect the daemon. These were replaced with raw asynchronous D-Bus messages, bounded timeouts, asynchronously obtained/cached daemon ownership, and suspended discovery during password entry/connection. The previous native app was retained for rollback. All three native test suites passed, and the delayed-service keyboard regression test also passed directly on the physical frame using an isolated private bus. The updated app was installed and restarted into Settings. A human typing check is still needed before calling the reported lag resolved.


## Persistent account release verified — 2026-10-14

The approved release is installed on SD Linux: Show password and Remember this account in iCloud sign-in; Change calendars, Replace password and Forget account; one-minute calendar change checks and a separate 15-minute weather timer. Wi-Fi password entry now opens asynchronously instead of nesting a dialog event loop, and checkbox hit targets cover the whole row. Native GUI and calendar connector run as separate restricted users; the GUI has no sudo access. The host-key encryption/removable-SD limit remains as discussed with the owner.

All four native suites passed on the final release: calendar change detection/fallback/rejection, credential destination and event parsing, Settings/touch/password controls, and delayed-iwd input responsiveness. Three Python credential-helper validation tests passed. Offline visual checks covered the expanded iCloud Settings pane in dark landscape and light portrait.

The real frame passed authenticated encryption, altered-data/wrong-name rejection, restricted systemd credential delivery, two service starts and a subsequent Linux reboot, using a separate synthetic credential. The production helper's Save and removal also passed in an isolated copy of its exact sandbox with a separate fixture path. The live production sockets rejected unapproved peers; the GUI could not read the host key or connect directly to the credential writer. A primary-password fixture was rejected without contacting Apple or saving an account. Empty-account Forget restarted only the calendar connector. All isolated credential fixtures and probe services were removed afterwards; the production host key was retained.

The first two installer attempts detected configuration-path/Python syntax errors and restored the previous dashboard automatically. The corrected sealed installer completed successfully; its embedded Python was syntax checked before the successful retry. A clean reboot verified active kiosk, rotation, credential writer, calendar service, iwd and firewall; Wi-Fi reconnected automatically with its private stable address. An app-only restart retained the calendar service's PID. Final memory readback showed 809 MiB available and zero swap use. Internal Android/eMMC, kernel and module versions were unchanged.

The account service is ready for one final on-frame sign-in with Remember this account checked and calendar selection. Earlier versions kept the owner's credential only in their GUI process; it was not extracted or transferred by diagnostics. Actual iCloud authentication, saving the owner's account and subsequent live-account restoration remain to be checked after that input. Settings is open for the owner. The preceding verification establishes implementation and fixture behavior, not a completed live-account login or a human confirmation that the remaining Wi-Fi typing symptom is resolved.


### AI family briefing installed (October 14, 2026)

The SD Linux frame now runs a compact native briefing below the weather. Responses
are bounded to 280 Unicode code points and additionally fitted to a fixed text
rectangle with the same wrapping policy used to draw them. Landscape, portrait,
dark mode, and an oversized unbroken-word preview were inspected. AI settings
include preferences, enable/pause, manual refresh, key removal and a local monthly
spending allowance; automatic generation remains disabled until enabled by the user.

The calendar service owns the OpenAI key and an independent upcoming-calendar
window. Key setup uses a hidden Mac terminal prompt and the verified USB console;
the receiver encrypts stdin to a fixed credential destination without plaintext
files, shell arguments or logs. An isolated fictional-key probe on the physical
frame verified no-echo input, encryption round trip, 0600 mode and TTY restoration.
No actual OpenAI key was installed or paid request made during this release.

All five ARM SDK test suites passed. The final briefing suite also passed all 12
checks on the frame, including calendar-view/weather recovery after a service
restart, durable cache/budget behavior, source outages, long text and privacy
boundaries. Installed app and calendar binaries were compared against the sealed
release. iCloud reauthenticated and the state directory is service-owned mode 0700.

The final service checks found that stopping the kiosk also stopped its dependent
orientation watcher; it was restarted, and the installer/rollback now explicitly
starts it. An invalid empty SetCredential fallback was replaced with a harmless
JSON sentinel and systemd syntax checked. Every dashboard, calendar, credential,
rotation, display and Wi-Fi service was then checked individually. Android, eMMC,
boot firmware and Wi-Fi configuration were not changed. Setup instructions are in
[linux/ai/README.md](../linux/ai/README.md).

### Approved native layout installed (October 14, 2026)

The approved HTML mockup was carried into the native Qt app, with the requested
bottom alignment for the Last updated section. The clock uses twelve-hour time
and a shared text baseline for AM/PM; the app header and timezone subtitle are
removed. Drawn conditions icons cover today and the next three forecast days.
Today/Tomorrow/Calendar and an icon-only Settings gear share one bottom row with
successful iCloud, weather, and summary refresh times. Landscape and portrait,
light and dark, and oversized briefing text were visually checked with fictional
data. The 280-character and fixed-pixel briefing bounds remain in force.

Settings adds Errors & debugging: source health, last successful checks, memory,
uptime, conservative AI spend, a bounded in-memory error history, retry, clear
resolved history, and an allowlisted diagnostic export. Credentials, account
names, network names, and event/briefing contents are excluded from the export.
The gear shows an active-issue badge. Calendar IPC now reports successful check
time and a fixed issue category; it does not expose new credentials.

All six native test suites passed in the ARM SDK and on the actual frame. The
final Settings label correction passed the affected on-frame suite again.
Installed binaries were compared to the sealed release. The saved live iCloud
account reauthenticated after the backend restart, weather connectivity returned,
and the enabled AI briefing retained its encrypted key and durable budget/cache.
The regular briefing flow produced a fresh result under the existing rate and
spending limits; no manual paid test request was made.

Every dashboard, calendar, credential, orientation, display, Wi-Fi, iwd, and
firewall unit was checked individually. The live fullscreen window matched
1280×800 and the current Sway orientation. Readback after installation showed
809 MiB available memory and zero swap use. Physical turning/tapping of this
release remains an owner check; the sensor and touch configuration are unchanged.

The installer saves previous binaries under
`/var/lib/porthole-install/before-native-layout`, with automatic failure rollback.
A separate app-only follow-up corrected Qt mnemonic escaping in the diagnostics
tab and retains its prior binary under `before-native-layout-label`. Credentials,
network profiles, kernel, boot firmware, and internal Android were not modified.


### Family briefing schedule and complete-text release (October 14, 2026)

The frame now uses morning (6 AM), midday (noon), and evening (4 PM) prompts,
with weekday/weekend focus and explicit household lunch/school, clothing-weather,
and advance-planning priorities. Extra saved preferences remain available.
Half-hour local checks through 9 PM compare stable source fingerprints; unchanged
checks make no OpenAI request. Overnight boot and manual refresh cannot start
a request. The durable daily cap is 31 attempts; the monthly spending limit and
pre-request reservation were retained.

A cache checked successfully within 30 minutes and belonging to the current
period is reused at restart. Older or missing caches generate a current-period
briefing after source recovery during waking hours. Failed checks cannot renew
cache freshness. Legacy preferences and spending migrate without reset; the
ledger remains compatible with rollback to the previous backend.

The briefing moved down 20 pixels and has a 166-pixel panel with a 134-pixel
text area. It shows all of a bounded 400-character response by adapting the font,
without appending an ellipsis. Oversized provider responses are rejected while
keeping the previous valid text. Full long-text landscape/portrait previews,
both themes, narrow widths, and unbroken-word stress were inspected/tested.
The original reported ellipsis was confirmed as display truncation: the saved
186-character briefing ended normally. Today's weather now displays sunrise
and sunset from the existing configured-timezone Open-Meteo query; forecast tiles
remain unchanged. The live endpoint returned today's solar times successfully.

All six final native suites passed in the ARM SDK and on the frame, including
21 checks in the briefing suite. Installed app/backend binaries matched the
sealed release; all calendar, credential, kiosk, orientation, display, Wi-Fi,
iwd and firewall services were checked individually. iCloud recovered and
the regular app flow generated a fresh midday briefing, with 4 PM as the next
check. A further calendar-service restart reused the exact briefing and saved
preferences, generation time, attempt counter and spend without an extra request.
Readback showed 808 MiB available and zero swap use.

Previous app/backend binaries are saved under
`/var/lib/porthole-install/before-briefing-schedule`. The SD-only installer retains
automatic rollback; credentials, Wi-Fi profiles, rotation configuration, kernel,
boot firmware and internal Android were not modified. See
[AI scheduling and setup](../linux/ai/README.md).

### Private household configuration and briefing grounding (October 14, 2026)

Weather location, coordinates, timezone and household context now use a validated
private profile, separate from publishable source. Settings → Data edits location
and timezone, and Settings → AI edits household context. The backend stores the
profile atomically with mode 0600 and delivers it through the restricted local
IPC. Empty configuration uses UTC and requests no guessed weather. Public source,
examples and previews use generic household/location data. Saved settings survived
a real Settings IPC save and calendar-service restart with identical cached
briefing, generation time, attempt counter and spend. Existing credentials recovered.

The briefing input now includes explicit local today/tomorrow dates and a weekday
label, relative day offset and inclusive last date for every calendar event.
Forecasts carry matching day labels and are explicitly daily highs/lows rather
than hourly observations. Updated prompts preserve event dates, use configured
adult/child roles and meal arrangements, avoid invented preparation tasks, and
prefer useful facts over empty-calendar commentary. Semicolon chains are converted
to sentences at the response boundary before checking the 400-character limit.
Personal household details remain in the ignored/private profile only.

Two regression checks failed against the prior implementation and passed with the
fix: Wednesday/Friday event and forecast date labeling, and semicolon-free sentence
rendering. All seven final native suites passed in the ARM SDK; the profile suite
(8 checks) and briefing suite (24 checks) also passed on the physical frame.
The SD-only release preserves the previous binaries/profile under
`/var/lib/porthole-install/before-briefing-grounding`, without rolling back or
resetting the spending ledger. A bytewise profile preflight initially stopped
before any installation because an IPC save changed JSON formatting. The retried
installer compares parsed settings instead, while still refusing actual changes
made after the bundle was prepared.

The installed binaries and private profile matched the sealed bundle, and all
nine runtime services were active. A fresh live briefing kept later school-lunch
and adult photo events on their correct weekday, stated the provided meal fact,
and used tomorrow's daily high/low forecast without invented preparation chores
or semicolons. A calendar-service restart reused that exact corrected text,
generation timestamp, attempt count and spending amount, with healthy source
status and no additional OpenAI request. Existing iCloud login and AI key recovered.

### Signed application update channel (October 14, 2026)

The application now has an outbound-only Wi-Fi update channel using a configured
public GitHub release repository and a locally enrolled RSA verification key.
Private signing material stays on the publishing machine outside Git. The
restricted downloader has no root privileges; the local installer independently
checks signatures, bounded archive/file hashes, platform and executable headers.
Only the two app binaries are eligible, with monotonically increasing sequences.
Profile, credentials, Wi-Fi, cache/ledger, firmware and kernel are preserved.

The tested Settings controls use the existing Data pane, with software status,
manual checking and an automatic-update toggle. Light landscape and dark portrait
previews were inspected, and all eight native suites plus nine updater security/
transaction tests passed. An initial bootstrap stopped before installation on a
relative sysusers path; it was corrected to use an absolute path and failure cleanup.
Initial signed probes were held back before replacing binaries because the Python
service did not retain its setuid capability. Explicit ambient inheritance and a
fixed-purpose `setpriv` identity drop corrected that sandbox boundary.

A private signed test app that passed its lightweight probe but failed GUI startup
was then installed on the actual frame. The installer detected the missing window,
restored both previous binaries exactly, removed the pending journal and restarted
calendar, kiosk and rotation successfully. The failed sequence was retained to
prevent automatic retries. No failed test release was published publicly.

The first production release (2026.10.14.1) was published with a signed manifest,
signature and app-only archive. The frame's unprivileged service fetched it through
Wi-Fi, and the independent installer accepted it after startup/window checks.
Both installed binaries matched the downloaded manifest. The private profile
matched its prior provisioned value; saved iCloud login/calendar selection and
OpenAI-key flags recovered, and the spending ledger retained nonzero history.
All eleven application, network, update and timer services were active. Automatic
checks were re-enabled after testing. No MAC/IP address or inbound listener was
needed, and no private key or household configuration was published.

### Wall-power startup and migration bootstrap (October 15, 2026)

The frame appeared on the Mac as an Allwinner USB download gadget exporting
only its SD card. The FAT boot state showed automatic mode, an unconfirmed
previous Linux start, and the current USB disk selection. Inspection of the
installed boot script confirmed that this fallback tests USB host configuration,
not dashboard health. A full power disconnect returned the next start to Linux.
The dashboard, calendar service, and updater were active, with app release
2026.10.14.7 installed. The original `uEnv.txt` was backed up privately on SD;
`pf_mode=linux` was written atomically and read back for standalone wall-power
operation. A subsequent clean Linux reboot returned to the native dashboard with
the preference retained, the pinned kernel unchanged, and the calendar, kiosk,
updater and check timer active. A physical wall-power-only start remains untested.

The checksum-covered credential migration bootstrap was transferred through the
matched Linux USB maintenance console and installed on the verified SD root and
pinned kernel. The existing key and updater settings were retained. The updater,
check timer, calendar service and kiosk were active afterwards. This establishes
bootstrap installation; a signed schema-2 migration over Wi-Fi remains to be tested.

### Calendar selection and touch controls (October 15, 2026)

Release 2026.10.15.1 installed over Wi-Fi, with both native binary hashes matching
the signed release. It replaces the calendar picker's small item check indicators
with full-row checkbox controls and enlarges checkbox indicators throughout the
app. Native UI tests cover text, indicator and blank-row taps, keyboard operation,
disabled controls and the selection limit; all eleven native test suites passed.

Before reselection, the live backend reported a remembered, authenticated iCloud
account with seven available calendars but no selected calendars. After the owner
saved six calendars using the new picker, the service reported a remembered
selection and no selection request. Both a calendar-service restart and a clean
frame reboot restored the same six-calendar selection, checked by a digest of
the selected URLs without recording calendar names or event data. The old loss
of selection was not reproduced after reselection; its original cause remains
unconfirmed. No speculative backend restoration change was made.
