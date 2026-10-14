# PF1007L Linux Wi-Fi

This SD-only setup is specific to the physically verified A33 PF1007L, kernel `7.2.10-1-armv7-ARCH`, and AIC8800DC SDIO identity `C8A1:C08D`. It uses the already installed, ABI-matched AIC modules and firmware. It does not change internal Android or eMMC.

`install-network.sh` installs the reviewed network configuration on SD, disables SSH, enables an inbound firewall, configures iwd DHCP/DNS integration, and generates a private stable locally administered MAC once. The MAC is assigned before iwd starts scanning or connecting. Root owns the MAC file and saved iwd profiles; their values are not part of the repository.

`wifi-start` verifies SD root, the pinned kernel and module versions, and the Wi-Fi chip identity. When no Wi-Fi adapter is bound, it resets only the identified `1c10000.mmc` Wi-Fi host before loading the modules. It checks that this host differs from the SD root host. This addresses a verified first-load timeout after the chip had remained enumerated without its driver for several hours. The successful host reset and firmware initialization created `wlan0` on the physical frame; no driver-source patch was needed. The underlying idle-card timeout cause is not yet established.

The native Settings → Wi-Fi picker talks to iwd over the system bus. It supports personal password-protected and open networks, with masked on-frame password entry. iwd remembers the connection on SD for later reconnecting. The application restricts password-agent replies to the actual iwd bus owner and the selected network. An unrelated root D-Bus request was rejected on the physical frame.

`firewall.nft` owns only the `inet porthole` table. Incoming established traffic, loopback, ICMP and DHCP replies are allowed; unsolicited incoming traffic and forwarding are dropped. Outgoing traffic is allowed for the user's calendar, weather and time services. mDNS and LLMNR are disabled. This does not by itself isolate the frame from other devices on the same LAN; a guest/IoT network can provide that separation.

No Wi-Fi password belongs in a script, command argument, repository, chat, diagnostic log or screenshot. Joining a real network requires the owner to select it and type its password on the frame. iCloud authentication is a separate Settings action and uses its dedicated app-specific password.
