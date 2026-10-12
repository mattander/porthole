# PF1007L access plan — approval required before execution

Research date: 2026-10-12. No access helper, payload, or device-changing command has been executed. No software has been installed and the frame has not been connected to Wi-Fi by this task.

## Identified unit

The parent's visual transcription of the user's photos establishes these non-unique details:

| Item | Value |
| --- | --- |
| Rear-label brand/model | Portrait / PF1007 |
| Input rating | DC 5V 2A |
| Settings model | PF1007L |
| AiMOR version | 6.0.78 |
| Firmware | YHK.F-null-A33-1001-D1024-PF1007L-321-OS-AIMOR.0.20260410 |
| Visible features | USB-C, TF slot, RESET pinhole |

The MAC/frame ID in the photo is excluded. The obscured FCC identifier is not guessed. A33 is a firmware-family lead; this unit's silicon, Android version, partition geometry, and actual storage capacity have not been queried.

## Host prerequisites checked

Python 3, clang, and make are available. ADB/Fastboot, sunxi-fel, pkg-config, Docker, and Colima are absent from PATH; the installed Homebrew inventory had no matching libusb/dtc/pkgconf/Colima/Docker packages. These are missing prerequisites, not a compatibility verdict. The network route needs Platform-Tools; the published offline build route needs additional dependencies/build tooling.

A read-only listener check found no current-user-visible listener on DNS port 53 or command port 7806. Recheck before an approved session; this was not a privileged/global listener audit. No host firewall, DNS, network, or service state was changed.

## Concrete access candidates

Sources were read at upstream revision [fbe9a30ac4f9c77d3863e19a5e98d9eadb04ab03](https://github.com/wesbos/photo-frame-dashboard/tree/fbe9a30ac4f9c77d3863e19a5e98d9eadb04ab03). GitHub's live API confirmed PRs 13 and 20 merged; PR 14 remained open. These are community engineering reports, not manufacturer guarantees.

### First: stock USB ADB

The [family test sequence](https://github.com/wesbos/photo-frame-dashboard/blob/fbe9a30ac4f9c77d3863e19a5e98d9eadb04ab03/docs/pf1007l-start-here.md) reports different ADB defaults even between units sharing firmware. Our current USB configuration exposes only Mass Storage, so ordinary USB ADB is unlikely to work immediately. After approval for official Platform-Tools, a bounded `adb devices -l` check can establish the baseline. Do not approve Android trust or enable debugging automatically.

If stock access is already available, use the local-only target selection with `scripts/inspect_android.py`. It does not run `adb root`, pair/connect networking, change properties, install apps, or reboot. It reads a chosen existing transport and omits its serial from output.

### Temporary root ADB: exact April firmware report

A [merged report](https://github.com/wesbos/photo-frame-dashboard/blob/fbe9a30ac4f9c77d3863e19a5e98d9eadb04ab03/docs/pf1007l-20260410-network-adb.md) matches our full firmware and AiMOR version. Its Mac test used a local stand-in for the vendor command connection and one runtime command:

```sh
setprop service.adb.tcp.port 5555; stop adbd; start adbd
```

It reported UID 0 without writing boot firmware; access ended at reboot. It also reported **unauthenticated root ADB reachable by other devices on the same network**. That is not safe on the home LAN.

I read the upstream `server.py`, `dns.py`, and `proto.py` without executing them. The listener restricts the source peer, waits for login, and dispatches a fixed command once. DNS redirects `imsconnect.efercro.com` to the host; it does not itself change network settings. This review is not verification on our frame.

Approval must cover tooling, an isolated frame-and-Mac network, frame-only DNS redirection, running the pinned reviewed helpers, and the exact runtime command. Verify no public vendor connection is allowed. Read identity and capture backups before any package or system changes; restore DNS before stopping the helper, then close temporary ADB by the agreed reboot/power cycle. Do not silently change the home router's DNS.

### Offline backup route: SD/FEL and RAM-loaded U-Boot

The [Mac U-Boot guide](https://github.com/wesbos/photo-frame-dashboard/blob/fbe9a30ac4f9c77d3863e19a5e98d9eadb04ab03/docs/allwinner-a33-mainline-uboot.md) reports a spare-card FEL stub followed by RAM-loaded, board-specific U-Boot exposing eMMC as a read-only USB disk. It is a useful offline candidate for a full stock backup before persistent changes. Its tested January unit differs from ours; recent reports flag UMS stability and write-guard issues.

This requires a spare microSD card/reader, host dependencies, and explicit approval for the card write and RAM payload execution. First verify A33 with a FEL version query, then actual eMMC size, media read-only status, and backup consistency. Stop before boot-patch generation or flashing unless separately approved. Do not use a January patched image or another unit's offsets blindly.

## Recovery and privacy limits

The [stock Android backup procedure](https://github.com/wesbos/photo-frame-dashboard/blob/fbe9a30ac4f9c77d3863e19a5e98d9eadb04ab03/docs/allwinner-a33-pf1007l.md#4-back-up-before-modifying-android) explicitly distinguishes read-only partition captures from a complete device backup. Preserve this unit's boot, recovery, system, storage layout, and relevant hardware boot areas; verify byte counts, independent stable rereads, hashes, and compressed readback. A live userdata capture may change while Android runs. Do not label a few partition files a complete rollback image. Keep all captures private.

[Community code-path research](https://github.com/wesbos/photo-frame-dashboard/blob/fbe9a30ac4f9c77d3863e19a5e98d9eadb04ab03/docs/aimor-remote-root-shell.md) describes AiMOR accepting a vendor shell command and passing it to `su` over an unencrypted connection, with a live April-firmware corroboration. This is a concrete reason to isolate the frame. It is not evidence that this user's unit has been used to spy; its installed executable code has not been independently inspected.

The reported RESET/TF route is not proof of a usable recovery procedure for this physical unit. No matching manufacturer factory-restoration package has been verified. Community reports say transient reset FEL, Android recovery, and Test App detours did not reliably provide ADB on the tested original PF1007L; they are not an immediate menu-unlock recipe.

## Clean OS and dashboard path

Root access and a replacement kiosk on stock Android have documented community precedent. A clean OS has not been established for this unit. The recovery U-Boot configuration disables display output because the panel GPIOs are unverified; successfully exporting eMMC would not prove Linux desktop/display compatibility.

After access and a verified rollback plan, inspect the board, panel/touch controllers, Wi-Fi chipset, boot restrictions, and stock configuration. Then assess a clean build with the required drivers. No generic Linux or Android image should be flashed based on the model string alone.

The practical next step is approval for official Platform-Tools and the stock ADB check. If no stock ADB exists, choose explicitly between the exact-firmware temporary route on an isolated network and the offline SD/FEL backup route. A dashboard install, vendor-package disabling, persistent ADB, and OS flashing remain later decisions.
