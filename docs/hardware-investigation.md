# Initial hardware investigation

Inspection date: 2026-10-12. Scope: non-destructive USB inventory and existing tooling on the connected Mac.

## Verified findings

| Check | Observed result |
| --- | --- |
| macOS | 27.0.1, build 26A434 |
| `system_profiler -listDataTypes` | Current USB datatype is `SPUSBHostDataType` |
| `system_profiler SPUSBHostDataType -json` | Three built-in USB 3.1 buses using AppleT8112USBXHCI; no device children |
| `ioreg -p IOUSB -w 0` | Root plus three AppleT8112USBXHCI controllers; no attached device nodes |
| `ioreg -r -c IOUSBHostDevice -l -w 0` | Empty output |
| `ioreg -r -c IOUSBHostInterface -l -w 0` | Empty output |
| Tool lookup | ADB, Fastboot, rkdeveloptool, Heimdall, and lsusb absent from PATH |
| Standard Android locations | No ADB/Fastboot in Homebrew or /usr/local executable locations or the usual Android SDK platform-tools location; no Android Studio at its standard application location |
| Installed Homebrew packages | No matching Android/USB tooling in formula/cask inventory |

The older `SPUSBDataType` returned an empty array on this Mac. The conclusion above uses the supported `SPUSBHostDataType` and independent I/O Registry checks.

No USB name, VID/PID, serial, model, firmware, Android version, board revision, or SoC could be read from the frame. No ADB, MTP, serial, or vendor loader interface was observed. Tool lookup was bounded to PATH and standard installation areas, not an exhaustive search of personal files.

The absence of enumeration does **not** establish a power-only port or incompatible frame. The cable, port role, boot state, USB configuration, and Mac accessory permission remain unresolved. The frame's actual Wi-Fi state was not observable through these diagnostics.

No device settings, firmware, trust permissions, or network configuration were changed. No software was installed. No evidence of spyware was established.

## Power baseline pending

A subsequent user report on 2026-10-12 says the supplied cable was replaced with an Apple USB-C cable and the frame then did not turn on. Cable endpoint types and the exact power arrangement have not yet been confirmed. This is a user-reported observation, not a Mac-measured hardware diagnosis.

Device probes are paused until normal power and boot are established. The next physical check is the original cable with the supplied wall adapter, following the labelled input requirements. Confirm the cable endpoints and rear-label power/port information before further data-connection tests. Do not infer damage, a power-only USB port, or a specific USB-C circuit defect from this observation. Do not improvise wiring or inject power.

The earlier USB snapshots do not establish whether the frame was fully powered at the time. Resume read-only enumeration only after boot is confirmed.

## Evidence from external sources

The [purchased ASIN's listing](https://www.amazon.ca/Digital-Picture-Storage-Auto-Rotate-Remotely/dp/B0H4W1YPHH) matches the stated brand, color, display, storage, and AiMOR software. It does not establish the physical unit's board or firmware. PF108 remains unconfirmed.

The supplied [AiMOR owner report](https://xdaforums.com/t/aimor-digital-photo-frame.4706158/post-89870521) is a lead for USB-C ADB access on another unit. A search snippet reported that ADB was already enabled; the exact post could not be fetched directly. The Android 6.0.1 detail was supplied with the report and was not independently verified here. It is not proof of compatibility with this frame.

[Apple documents](https://support.apple.com/en-us/102282) that an unapproved USB accessory can receive power while data access remains blocked on an Apple silicon Mac laptop. A permission prompt has not been confirmed in this investigation.

[Google documents ADB](https://developer.android.com/tools/adb) as a route for Android device enumeration and shell access. This requires a functioning connection and suitable device configuration/authorization; none is established for this frame yet.

## Immediate next steps

1. Confirm the frame is fully booted. Read its rear label and manual for exact model/revision, power requirements, and USB port roles. View Settings/About, if available, and record the complete firmware/build string without enabling developer options or connecting Wi-Fi.
2. Test a known working USB data cable directly to the Mac, following the documented power arrangement. Report any Mac accessory prompt for an explicit decision; do not approve automatically or weaken the global accessory policy.
3. Rerun `python3 scripts/inspect_usb.py`. If enumeration is still absent, resolve the documented port/cable arrangement before attempting flashing or loader utilities.
4. Obtain approval to download and extract Google's [Mac SDK Platform-Tools](https://developer.android.com/tools/releases/platform-tools) into a dedicated local directory. A full Android Studio installation is unnecessary for ADB. Tool installation alone cannot fix missing USB enumeration.
5. With tooling approved and USB present, run `adb devices -l`. Stop for a decision if debugging must be enabled or device authorization is requested. Do not approve persistent trust automatically.
6. If the frame is already accessible, explicitly select its confirmed serial locally with `adb -s <frame-serial>` and read selected properties: product model/device/board, hardware/platform, build fingerprint, Android release/SDK, verified boot state, and flash lock state. Read kernel and CPU/partition information where permitted. Sanitize findings before committing. Do not run `adb root`, reboot into recovery, install apps, or mutate settings under this inspection scope.

No alternative access or recovery route is verified for this exact unit. If documented software access remains unavailable after identification, seek a model-specific service manual and recovery package from the manufacturer or seller. A documented SD/USB photo-import feature could support a manually refreshed dashboard image, but would not prove APK installation or live updates.

## Dashboard and clean-OS feasibility

The next milestone is a reliable data connection and the full hardware/build identity. If Android app access is supported, an offline 1280×800 kiosk dashboard test could follow after installation approval. Live calendar/weather updates need a separately approved data-delivery design. A kiosk running on stock firmware does not satisfy the clean-OS privacy goal by itself.

Clean-OS feasibility is **undetermined**. ADB access alone would not establish that replacing the OS is possible. Before any destructive change, identify the exact board/SoC, storage layout, boot restrictions, display/touch hardware support, a trusted compatible replacement image, and a matching original-firmware backup or factory image with a documented recovery process. Do not assume a generic Android, LineageOS, or Linux image will boot.

Raw device evidence remains local and is deliberately excluded from this repository.
