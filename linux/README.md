# Porthole Linux display startup

Original Porthole code for the validated PF1007L SD-only Linux image, using kernel `7.2.10-1-armv7-ARCH`. These files are not a generic Linux image installer. See [the hardware bring-up record](../docs/linux-sd-bringup.md) for the source pins, board match, verification, and remaining checks.

`display-start` checks that root is on an SD card and that no eMMC block device appears, then loads the display modules in the order tested on the frame. It does not load Wi-Fi or the GPU. `systemd/porthole-display.service` runs it after local filesystems, udev, and the USB recovery service. The installed service passed shell/unit checks, ran successfully, and survived a clean reboot with the panel connected and framebuffer present.

The runtime paths are `/usr/local/lib/porthole/display-start` and `/etc/systemd/system/porthole-display.service`. The selected SD boot label remains `full-stage1`, preserving its module blacklists and USB serial recovery configuration. Vendor firmware, generated images, and SSH keys stay in ignored local storage. Physical touchscreen events are verified. Display, touch alignment and all four orientation transforms have since passed physical checks. The current native dashboard replaces the temporary browser kiosk; see [native setup](native/README.md).
