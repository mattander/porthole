# Privacy and hardening plan

Recorded 2026-10-13 while the offline read-only backup was running. This is a plan; no networking, package changes, persistent debugging, root installation, or internal-storage writes are authorized by this document.

## User-supplied reference and evidence limits

The user supplied [NeoElevate_'s A33 frame hardening thread](https://x.com/NeoElevate_/status/2109761532213223518), posted 2026-10-12, and subsequently pasted the opening and all five replies after web viewers failed to expose them. The owner reports:

- NetAlertX detected an additional MAC address after reboots. Network rules blocked its internet access and also access to the owner's local NTP server, causing clock problems.
- The extra address was temporary during boot, before `wpa_supplicant` started and the usual address took over. Two addresses were observed per boot.
- Pinning the address across the boot sequence restored the intended DHCP reservation and time synchronization.
- Boot was reduced to just under 60 seconds, and a custom boot animation was added. Further boot improvements and a doorbell feed were future goals.

The pasted thread does not provide the exact firmware, Wi-Fi chipset, pinning commands, packet capture or measured boot stages. These are another owner's observations, not verified behavior on our unit. The transient address alone does not establish malicious intent. Do not apply an unspecified MAC fix blindly.

Our chip is directly verified as Allwinner A33 through FEL. Stock USB ADB was absent. The prepared SD and RAM-only exporter expose storage with both sector writes refused and LUN write protection enabled. The lower-clock build passed two independent 256 MiB reads; the 15,758,000,128-byte eMMC user-area backup is now verified: both independent full-device reads, saved-image hashing and compressed readback passed. An actual restore has not been tested. Separate eMMC hardware boot areas are not included in the current export.

The [exact April-firmware community report](https://github.com/wesbos/photo-frame-dashboard/blob/fbe9a30ac4f9c77d3863e19a5e98d9eadb04ab03/docs/pf1007l-20260410-network-adb.md) and [AiMOR command-channel analysis](https://github.com/wesbos/photo-frame-dashboard/blob/fbe9a30ac4f9c77d3863e19a5e98d9eadb04ab03/docs/aimor-remote-root-shell.md) describe a vendor-command path and unauthenticated root network ADB on another unit. They have not been exercised or independently confirmed on ours. No evidence of spying on this unit has been established.

## Requirements for the eventual dashboard

- Keep the current backup session offline. Finish the independent device reread and host/compressed readback before treating its user-area image as verified; establish recovery coverage separately before any firmware change.
- Inspect the verified backup offline for vendor command services, update clients, boot-started services, ADB authentication/defaults, and Wi-Fi driver/address configuration. Record findings separately from community claims. Do not execute extracted vendor programs on the Mac.
- For any later approved network test, use an isolated network with enforcement at its interface or network segment. Check both IPv4 and IPv6, blocked access to the trusted LAN, and blocked general internet/vendor traffic. A different SSID alone does not demonstrate isolation.
- Investigate the early-boot versus post-`wpa_supplicant` address on this hardware. Inspect driver/startup configuration offline first; in a later approved isolated test, compare address and DHCP behavior from power-on through association across repeated cold boots. If it occurs, propose a chipset-compatible address configuration that applies before interface activation, with verification and rollback. Do not assume an Android privacy toggle fixes driver/startup behavior.
- Avoid making containment depend solely on one MAC address or a DHCP reservation. Keep network-segment rules effective even when an unexpected address appears; verify both IPv4 and IPv6 paths.
- Include time synchronization in the allowed-services design. Select an approved local NTP endpoint (commonly UDP 123), or another verified clock source, alongside the dashboard service and required local DHCP/DNS. Verify actual synchronization after cold boot while general internet/vendor access remains blocked. Check calendar dates and HTTPS behavior with the corrected clock. This proposes no current network change or existing local NTP server.
- Prefer a display that can reach only the intended dashboard service. Keep calendar account credentials and weather API secrets on the data-serving host; provide only the data and narrowly scoped display access it needs. Verify actual traffic in an isolated test.
- Do not leave unauthenticated root ADB or other administration services reachable on a normal network. Inspect and plan the administration route before enabling it, including authentication, reachability and how it is closed.
- If stock Android must be retained, inspect the vendor packages and dependencies before proposing their removal or disabling. A new launcher alone does not remove background vendor services. Package or boot changes require their separate approved implementation and rollback plan.
- Treat sub-60-second boot and a custom animation as later usability goals. Measure this unit's baseline before optimizing; neither feature establishes privacy or a clean OS. A doorbell feed is optional future work outside the current calendar/weather scope.
- Continue evaluating a clean OS using this unit's panel, touch, Wi-Fi and boot evidence. A successful read-only exporter or a dashboard demonstration on another A33 unit is not sufficient compatibility proof.

The access and recovery constraints remain in [the PF1007L access plan](pf1007l-access-plan.md). Raw backups, identifiers, tokens and diagnostic logs remain ignored and private.
