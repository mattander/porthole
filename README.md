# Porthole

An open-source project to turn a digital photo frame into a family calendar and weather dashboard.

The first target is an Anna Bella frame sold through Amazon.ca as ASIN B0H4W1YPHH: black with white border, 10.1-inch 1280×800 display, 32 GB, and AiMOR software. These are [listing specifications](https://www.amazon.ca/Digital-Picture-Storage-Auto-Rotate-Remotely/dp/B0H4W1YPHH), not an identification of the connected hardware. The physical unit is labelled Portrait PF1007, with PF1007L shown in Settings; the initial PF108 guess is superseded.

## Current status

After changing to a working USB-A-to-C connection and accepting the Mac accessory prompt, the frame now enumerates as **Android / USB Developer, USB `1f3a:1000`**, at 480 Mb/s. Its current configuration exposes USB Mass Storage (`08/06/50`), with no ADB interface observed. No block device was found under the USB branch. Photos identify PF1007L, AiMOR 6.0.78, and April 20260410 firmware. Android version and silicon remain unqueried. ADB and Fastboot are absent from the checked installation locations. No firmware or device settings have been changed.

See [the investigation record](docs/hardware-investigation.md) for verified findings, limitations, and the next steps. The [PF1007L access plan](docs/pf1007l-access-plan.md) identifies an exact-firmware community root route and an offline backup candidate, both awaiting approval. A custom dashboard is not implemented yet; clean-OS compatibility and recovery remain unverified.

## Inspect USB safely

Run the standard-library-only diagnostic on macOS:

```sh
python3 scripts/inspect_usb.py
```

It reads host USB inventory and prints a sanitized JSON summary. It does not issue commands to the frame, run ADB, install software, or save raw output. Device serials, registry IDs, host usernames, and executable paths are omitted. The summary includes USB vendor/product IDs and interface class numbers when available; those are hardware descriptors, not unique device identifiers.

After approved ADB access exists, `python3 scripts/inspect_android.py --serial <locally-verified-transport>` reads selected model/build/CPU/partition facts from that transport. It never prints the target serial, queries unique identifiers, or enables root/debugging.

Keep the frame off home Wi-Fi during investigation. Enabling debugging, authorizing device trust, installing software, unlocking, rooting, resetting, or flashing requires a separate explicit decision. Read-only inventory does not authorize those actions.

## Privacy and contributions

Commit reproducible code and sanitized findings. Keep raw diagnostic dumps, device serials, private calendar data, credentials, and local configuration outside Git. Use the ignored `local/` directory for private working files. Inspect staged changes before publishing.

## License

[MIT](LICENSE), copyright 2026 Matt Anderson.
