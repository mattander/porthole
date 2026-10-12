# PF1007L access plan — approval required before execution

Research date: 2026-10-12. No access helper, payload, or device-changing command has been executed. Official Google Platform-Tools has subsequently been installed with user approval. The frame has not been connected to Wi-Fi by this task.

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

Python 3, clang, and make are available. Platform-Tools 37.0.1 is installed in ignored `local/android-platform-tools/platform-tools/`, without a PATH change. The targeted stock check returned zero ADB devices and the task-local server was stopped. sunxi-fel, pkg-config, Docker, and Colima are absent from PATH; the installed Homebrew inventory had no matching libusb/dtc/pkgconf/Colima/Docker packages. These are missing prerequisites, not a compatibility verdict. The network route has its ADB prerequisite now; the published offline build route still needs additional dependencies/build tooling.

A read-only listener check found no current-user-visible listener on DNS port 53 or command port 7806. Recheck before an approved session; this was not a privileged/global listener audit. No host firewall, DNS, network, or service state was changed.

## Concrete access candidates

Sources were read at upstream revision [fbe9a30ac4f9c77d3863e19a5e98d9eadb04ab03](https://github.com/wesbos/photo-frame-dashboard/tree/fbe9a30ac4f9c77d3863e19a5e98d9eadb04ab03). GitHub's live API confirmed PRs 13 and 20 merged; PR 14 remained open. The exact-firmware document was retrieved and decoded through the REST contents API, with blob SHA `2d554b792b2f8992df17b2d838d728868a45a618`. PR 20 merged at 2026-10-12T17:38:27Z, merge commit `8ade47d050a4511961e600f0aa04eab838793f51`. This evidence is from actual API responses, not search snippets. These are community engineering reports, not manufacturer guarantees.

### First: stock USB ADB

The [family test sequence](https://github.com/wesbos/photo-frame-dashboard/blob/fbe9a30ac4f9c77d3863e19a5e98d9eadb04ab03/docs/pf1007l-start-here.md) reports different ADB defaults even between units sharing firmware. Our current USB configuration exposes only Mass Storage, so ordinary USB ADB is unlikely to work immediately. That approved, targeted baseline check has now returned zero devices. Do not approve Android trust or enable debugging automatically.

If stock access is already available, use the local-only target selection with `scripts/inspect_android.py`. It does not run `adb root`, pair/connect networking, change properties, install apps, or reboot. It reads a chosen existing transport and omits its serial from output.

### Temporary root ADB: exact April firmware report

A [merged report](https://github.com/wesbos/photo-frame-dashboard/blob/fbe9a30ac4f9c77d3863e19a5e98d9eadb04ab03/docs/pf1007l-20260410-network-adb.md) matches our full firmware and AiMOR version. Its Mac test used a local stand-in for the vendor command connection and one runtime command:

```sh
setprop service.adb.tcp.port 5555; stop adbd; start adbd
```

It reported UID 0 without writing boot firmware; access ended at reboot. The listener permits 1–600 seconds (300 default), but that timeout does not close ADB; the reported daemon remains active until reboot. It also reported **unauthenticated root ADB reachable by other devices on the same network**. That is not safe on the home LAN.

I read the upstream `server.py`, `dns.py`, and `proto.py` without executing them. The listener restricts the source peer, waits for login, and dispatches a fixed command once. DNS redirects `imsconnect.efercro.com` to the host; it does not itself change network settings. This review is not verification on our frame.

Remaining approval must cover an isolated frame-and-Mac network, frame-only DNS redirection, running the pinned reviewed helpers, and the exact runtime command. Verify no public vendor connection is allowed. Read identity and capture backups before any package or system changes; restore DNS before stopping the helper, then close temporary ADB by the agreed reboot/power cycle. Do not silently change the home router's DNS.

### Isolation has not been established

The Mac registers Wi-Fi, Ethernet adapter, and Thunderbolt interface types. That does not establish the connected equipment or an isolated topology. Apple's [Internet Sharing guide](https://support.apple.com/guide/mac-help/share-internet-connection-mac-network-users-mchlp1540/mac) documents connection sharing, not a verified security boundary for this task. No Sharing, routing, firewall, or frame network configuration was changed.

A separate user-owned access point with its WAN disconnected, or a verified isolated router/VLAN permitting only the frame and administering Mac, is a candidate. The availability of that equipment is unknown. A different SSID alone is insufficient; verify that the frame cannot reach trusted LAN devices or the public vendor service before executing the runtime command. Mac-only offline Wi-Fi hosting has not been validated here. The report redirects one hostname while its helper forwards unrelated DNS publicly; entirely WAN-free AiMOR login has not been verified for our unit. Do not silently enable vendor/internet access to make a test pass.

### Offline backup route: SD/FEL and RAM-loaded U-Boot

The [Mac U-Boot guide](https://github.com/wesbos/photo-frame-dashboard/blob/fbe9a30ac4f9c77d3863e19a5e98d9eadb04ab03/docs/allwinner-a33-mainline-uboot.md) reports a spare-card FEL stub followed by RAM-loaded, board-specific U-Boot exposing eMMC as a read-only USB disk. It is a useful offline candidate for a full stock backup before persistent changes. Its tested January unit differs from ours; recent reports flag UMS stability and write-guard issues.

I read the backup patch and helper implementation without running them. UMS sector writes are patched to return failure, and its LUN is advertised read-only; the host backup helper also rejects writable media and independently rereads data. These are useful safeguards, not a compatibility proof for our board.

This requires a spare microSD card/reader, host dependencies, and explicit approval for the card write and RAM payload execution. It can avoid internal eMMC writes by stopping at the read-only export stage, but it is not a zero-write procedure: it writes the spare card and uploads/executes RAM code. First verify A33 with a FEL version query, then actual eMMC size, media read-only status, and backup consistency. Stop before boot-patch generation or flashing unless separately approved. Do not use a January patched image or another unit's offsets blindly.

### USB ADB without a network

The retrieved `scripts/a33-uboot/boot-patch.py` (blob `9339dd88766cd3119b451cfd265a2abb514961f2`) replaces the stock mass-storage action in `init.sun8i.usb.rc`: it selects USB product 1002, enables `mass_storage,adb`, and starts `adbd`. That is a documented network-free USB-ADB path after an offline boot patch, but it requires an internal boot-partition write after a verified backup. It is not a stock read-only unlock. No verified USB-only runtime entry has been established on this unit.

## Recovery and privacy limits

The [stock Android backup procedure](https://github.com/wesbos/photo-frame-dashboard/blob/fbe9a30ac4f9c77d3863e19a5e98d9eadb04ab03/docs/allwinner-a33-pf1007l.md#4-back-up-before-modifying-android) explicitly distinguishes read-only partition captures from a complete device backup. Preserve this unit's boot, recovery, system, storage layout, and relevant hardware boot areas; verify byte counts, independent stable rereads, hashes, and compressed readback. A live userdata capture may change while Android runs. Do not label a few partition files a complete rollback image. U-Boot [UMS](https://docs.u-boot.org/en/latest/usage/cmd/ums.html) exports the selected block device; coverage of any separate eMMC hardware boot areas and a usable restore path still needs verification on this unit. Keep all captures private.

[Community code-path research](https://github.com/wesbos/photo-frame-dashboard/blob/fbe9a30ac4f9c77d3863e19a5e98d9eadb04ab03/docs/aimor-remote-root-shell.md) describes AiMOR accepting a vendor shell command and passing it to `su` over an unencrypted connection, with a live April-firmware corroboration. This is a concrete reason to isolate the frame. It is not evidence that this user's unit has been used to spy; its installed executable code has not been independently inspected.

The reported RESET/TF route is not proof of a usable recovery procedure for this physical unit. No matching manufacturer factory-restoration package has been verified. Community reports say transient reset FEL, Android recovery, and Test App detours did not reliably provide ADB on the tested original PF1007L; they are not an immediate menu-unlock recipe.

## Clean OS and dashboard path

Root access and a replacement kiosk on stock Android have documented community precedent. A clean OS has not been established for this unit. The recovery U-Boot configuration disables display output because the panel GPIOs are unverified; successfully exporting eMMC would not prove Linux desktop/display compatibility.

After access and a verified rollback plan, inspect the board, panel/touch controllers, Wi-Fi chipset, boot restrictions, and stock configuration. Then assess a clean build with the required drivers. No generic Linux or Android image should be flashed based on the model string alone.

The approved Platform-Tools installation and stock ADB check are complete, with no ADB endpoint found. The practical next decision is explicitly between the exact-firmware temporary route on an isolated network and the offline SD/FEL backup route. A dashboard install, vendor-package disabling, persistent ADB, and OS flashing remain later decisions.
