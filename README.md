# Porthole

An open-source project to turn a digital photo frame into a family calendar and weather dashboard.

The first target is an Anna Bella frame sold through Amazon.ca as ASIN B0H4W1YPHH: black with white border, 10.1-inch 1280×800 display, 32 GB, and AiMOR software. These are [listing specifications](https://www.amazon.ca/Digital-Picture-Storage-Auto-Rotate-Remotely/dp/B0H4W1YPHH), not an identification of the connected hardware. The physical unit is labelled Portrait PF1007, with PF1007L shown in Settings; the initial PF108 guess is superseded.

## Current status

The physical unit is PF1007L, AiMOR 6.0.78, with April 20260410 firmware. FEL directly identifies its silicon as **Allwinner A33**. The initial stock USB connection exposed Mass Storage (`1f3a:1000`) and no ADB device; the approved offline route now provides a RAM-only, write-protected storage export.

A **15,758,000,128-byte eMMC user-area backup is verified**: two independent full-device reads matched, the saved image matched, and compressed readback passed. The lower-clock RAM build also passed two independent 256 MiB preliminary reads. The spare SD card's original prefix was backed up and its 8 KiB FEL stub write was read back. No internal storage writes were sent, and no frame networking was enabled by this task.

This backup covers the exposed eMMC user area. Separate hardware boot areas and an actual restore remain unverified. Raw images, hashes and logs stay private in ignored directories. Android release, board/radio/panel details and clean-OS compatibility still need offline inspection. A custom dashboard is not implemented yet.

See [the investigation record](docs/hardware-investigation.md), [the access and recovery plan](docs/pf1007l-access-plan.md), and [the privacy and hardening plan](docs/privacy-and-hardening-plan.md). Persistent ADB, package changes, network setup and OS flashing remain separate decisions.

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
