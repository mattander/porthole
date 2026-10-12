# Porthole

An open-source project to turn a digital photo frame into a family calendar and weather dashboard.

The first target is an Anna Bella frame sold through Amazon.ca as ASIN B0H4W1YPHH: black with white border, 10.1-inch 1280×800 display, 32 GB, and AiMOR software. These are [listing specifications](https://www.amazon.ca/Digital-Picture-Storage-Auto-Rotate-Remotely/dp/B0H4W1YPHH), not an identification of the connected hardware. PF108 is a candidate model, still unconfirmed.

## Current status

Initial read-only inspection on 2026-10-12 found **no connected USB device or interface**. The Mac exposes three built-in USB controllers, but the frame does not enumerate. ADB and Fastboot were not found in PATH or the standard installation locations checked. No firmware or device settings have been changed.

Device probes are currently paused pending a power/boot check after a reported cable change.

See [the investigation record](docs/hardware-investigation.md) for verified findings, limitations, and the next steps. A custom dashboard is not implemented yet; clean-OS compatibility and recovery remain unverified.

## Inspect USB safely

Run the standard-library-only diagnostic on macOS:

```sh
python3 scripts/inspect_usb.py
```

It reads host USB inventory and prints a sanitized JSON summary. It does not issue commands to the frame, run ADB, install software, or save raw output. Device serials, registry IDs, host usernames, and executable paths are omitted. The summary includes USB vendor/product IDs and interface class numbers when available; those are hardware descriptors, not unique device identifiers.

Keep the frame off home Wi-Fi during investigation. Enabling debugging, authorizing device trust, installing software, unlocking, rooting, resetting, or flashing requires a separate explicit decision. Read-only inventory does not authorize those actions.

## Privacy and contributions

Commit reproducible code and sanitized findings. Keep raw diagnostic dumps, device serials, private calendar data, credentials, and local configuration outside Git. Use the ignored `local/` directory for private working files. Inspect staged changes before publishing.

## License

[MIT](LICENSE), copyright 2026 Matt Anderson.
