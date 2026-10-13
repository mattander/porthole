# Initial hardware investigation

Inspection date: 2026-10-12. Scope: non-destructive USB inventory and existing tooling on the connected Mac.

## Initial snapshot before the working data connection

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

## Verified USB enumeration after accessory approval

After a different A-to-C cable triggered macOS accessory consent and the user accepted it, read-only queries on 2026-10-12 found:

| Descriptor | Observed value |
| --- | --- |
| USB product string | Android |
| USB manufacturer string | USB Developer |
| Vendor/product ID | 1f3a:1000 |
| USB release descriptor | 2.00 |
| Device release descriptor | 0x0233; not an Android or firmware version |
| Negotiated link speed | 480 Mb/s |
| Exposed interface | Mass Storage, interface 0 |
| Interface class/subclass/protocol | 08/06/50: mass storage, SCSI transparent, bulk-only transport |
| ADB interface | None observed in this configuration |
| Block-device properties | No BSD Name/media-size properties found under the device's USB registry branch |

The interface base class is defined by the [USB-IF class-code specification](https://www.usb.org/defined-class-codes).

This confirms a USB data connection through the current cable arrangement. It does not establish an accessible filesystem, ADB availability in another device mode, exact SoC, or firmware compatibility. The generic USB strings do not identify the retail frame model. The release descriptor must not be interpreted as Android 2.3.3. No frame storage was mounted or read, and no USB mode was changed.

The diagnostic initially failed when using I/O Registry archive output with the connected device (`ioreg: error: can't open file`). Text-mode inventory succeeded. The script now reads text output and retains only integer descriptor fields from the matching device/interface nodes; serials and raw driver properties are discarded.

## Identity from the user's photos

The parent visually inspected the supplied photos and transcribed: Portrait PF1007 on the rear label, DC 5V 2A, PF1007L in Settings, AiMOR 6.0.78, and firmware `YHK.F-null-A33-1001-D1024-PF1007L-321-OS-AIMOR.0.20260410`. The frame has a RESET pinhole and TF slot. MAC/frame IDs were excluded, and the obscured FCC identifier was not guessed.

This establishes the displayed model/build identity. A33 in the firmware is a chip-family clue, not silicon verification. Android release, actual eMMC capacity, partition geometry, and PCB revision remain unread. The [access plan](pf1007l-access-plan.md) records matching community reports and concrete approval-gated options.

## Approved stock ADB check

The user approved official Google Android Platform-Tools installation and checking stock ADB. The vendor distribution was downloaded directly from `https://dl.google.com/android/repository/platform-tools-latest-darwin.zip`, its ZIP integrity checked, and its bundled notices inspected. The archive reports revision 37.0.1; ADB reports 37.0.1-15733141. Files remain in ignored `local/android-platform-tools/`; no global PATH or package-manager installation was needed.

A separate task-local ADB server was restricted with `--one-device` to the confirmed frame's USB serial, which was kept private. Host state and the server socket were task-local, network autodiscovery/autoconnect were disabled, and no Android trust was approved. `adb devices -l` returned zero devices. The task server was stopped afterward.

This confirms that ordinary stock USB ADB is unavailable in the observed configuration; it does not rule out another approved access route. No root, network, debugging, USB-mode, or firmware change was made.

## Cable and power observations

A subsequent user report on 2026-10-12 says the supplied cable was replaced with an Apple USB-C cable and the frame then did not turn on. The user subsequently confirmed that the supplied cable is USB-A to USB-C and the Apple cable is USB-C to USB-C. Boot with the original cable and supplied wall adapter is still awaiting confirmation. This is a user-reported observation, not a Mac-measured hardware diagnosis.

Read-only probes resumed after the user approved the Mac accessory prompt on a different A-to-C cable; USB now enumerates as documented above. The earlier wall-adapter baseline boot check was not reported back. Subsequent photos showed Settings/About and the rear-label details recorded above. A working power cable does not establish data capability. Do not infer damage, a power-only USB port, or a specific USB-C circuit defect from this observation. Do not improvise wiring or inject power.

The earlier empty USB snapshots do not establish whether the frame was fully powered at that time. The later successful enumeration supersedes the initial connection blocker.

## Evidence from external sources

The [purchased ASIN's listing](https://www.amazon.ca/Digital-Picture-Storage-Auto-Rotate-Remotely/dp/B0H4W1YPHH) matches the stated brand, color, display, storage, and AiMOR software. It does not establish the physical unit's board or firmware. The initial PF108 guess is superseded by the photographed PF1007/PF1007L labels.

The supplied [AiMOR owner report](https://xdaforums.com/t/aimor-digital-photo-frame.4706158/post-89870521) is a lead for USB-C ADB access on another unit. A search snippet reported that ADB was already enabled; the exact post could not be fetched directly. The Android 6.0.1 detail was supplied with the report and was not independently verified here. It is not proof of compatibility with this frame.

[Apple documents](https://support.apple.com/en-us/102282) that an unapproved USB accessory can receive power while data access remains blocked on an Apple silicon Mac laptop. The user subsequently confirmed accepting the Mac accessory prompt on the working connection.

[Google documents ADB](https://developer.android.com/tools/adb) as a route for Android device enumeration and shell access. This requires a functioning connection and suitable device configuration/authorization; none is established for this frame yet.

## Immediate next steps

The full next-session procedure is in the [PF1007L access plan](pf1007l-access-plan.md). Official Platform-Tools installation and the stock USB ADB check are complete; no ADB device appeared. Next, the matching April-firmware community report provides a temporary network-root candidate; an SD/FEL route provides an offline full-backup candidate. Both require explicit approval before execution. Neither root/recovery route has been tested on this physical unit.

Do not treat the user's accepted Mac accessory prompt as Android debugging trust or firmware-write approval. Keep networking, payload execution, and persistent device changes behind their own decisions. Do not use another unit's boot image, sector offsets, or storage capacity as this unit's verified values.

## Dashboard and clean-OS feasibility

A reliable USB data connection is established. The displayed model/build is identified; the next milestone is software access and direct hardware/partition queries. If Android app access is supported, an offline 1280×800 kiosk dashboard test could follow after installation approval. Live calendar/weather updates need a separately approved data-delivery design. A kiosk running on stock firmware does not satisfy the clean-OS privacy goal by itself.

Clean-OS feasibility is **undetermined**. ADB access alone would not establish that replacing the OS is possible. Before any destructive change, identify the exact board/SoC, storage layout, boot restrictions, display/touch hardware support, a trusted compatible replacement image, and a matching original-firmware backup or factory image with a documented recovery process. Do not assume a generic Android, LineageOS, or Linux image will boot.

Raw device evidence remains local and is deliberately excluded from this repository.

## Verified offline backup milestone — 2026-10-13

With the user's approval, the spare SD card's original 4 MiB prefix was preserved, an 8 KiB FEL stub was written at byte offset 8192, and its prepared prefix was independently read back before ejecting. Inserting it into the frame exposed FEL (`1f3a:efe8`); `sunxi-fel version` directly identified **A33, SoC 00001667**.

Pinned U-Boot v2025.10 source was built with sector writes refused and LUN write protection enabled. The initial exporter lost its disks on macOS. The reviewed START STOP UNIT correction and null-console input guard provided a stable idle export, but a full read later failed after 40 MiB. The frame's whole USB device disappeared. Scoped host logs did not establish a cause; no internal writes or capture-initiated unmount/eject occurred during that attempt. The failed partial image and logs were preserved privately.

A separate RAM-only build used the [matching upstream report's settings](https://github.com/wesbos/photo-frame-dashboard/issues/16): CPU 408 MHz, PMIC changes skipped, and eMMC 4-bit/26 MHz, retaining both write protections and the macOS stability correction. This succeeded over the user's direct Mac connection. Lowering the settings resolved the observed read failure; a power-delivery cause was not measured or established.

The export provided a 32,018,268,160-byte spare SD and a **15,758,000,128-byte eMMC user area**, both external USB whole disks with 512-byte sectors and `WritableMedia=false`. Before capture, the SD prefix fingerprint and stock Android boot header were read twice and matched; the expected eMMC boot-partition offset was independently checked. The new build's two preliminary 256 MiB reads matched.

The full eMMC capture is now verified: two independent full-device reads agreed, the saved image hash matched, no suspicious 0xCC filler was detected by the reviewed helper, and compressed readback matched. The completed image has the exact expected byte count; the compressed image and private hash record are present, permissions are 0600, and no partial image remains in the active backup directory. Raw images, unique identifiers and whole-device hashes are excluded from Git.

This is a verified backup of the exposed eMMC user area, not a proven complete rollback package. Separate hardware boot areas and an actual restore remain unverified. No internal storage writes were sent, no persistent root/debugging or package changes were made, and the task did not enable frame networking. The next authorized work is offline inspection of the verified image and recovery research. Firmware/package changes and networking still require their separate decisions.
