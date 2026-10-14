# Temporary Linux browser hardware test

This is an offline test of the existing frontend on the PF1007L SD-only Linux system. Firefox and Sway are installed from signature-verified Arch Linux ARM packages without changing the pinned kernel. The local HTTP server binds only to `127.0.0.1`; the frame has no Wi-Fi or USB-network interface. The UI uses explicit fictional sample data and contains no iCloud credentials.

Sway selects the verified panel at native 800×1280, rotates it 270 degrees to a 1280×800 landscape workspace, and uses the separately probed Mali render node. Actual frame screenshots and physical tab checks verified rendering and touch alignment. The browser profile lives under `/run` and is recreated each boot. Kiosk autostart is now enabled and has passed a clean reboot.

The deployed sample snapshot includes test-only click diagnostics, recording coordinates, viewport size, and element IDs/roles to `/run/porthole-web/touch-events.jsonl`. It does not record displayed calendar text, keyboard input, or passwords. The frontend worker's source files are unchanged. The diagnostics were removed after touch validation; their private evidence is retained.

The owner has requested a [native Linux application with iCloud authentication](../../docs/native-linux-app-plan.md) after this test. The browser is a temporary validation path, not the final native implementation.


The verified SC7A20 accelerometer uses I2C1 address 0x19. `orientation-sensor-start` selects its supported 25 Hz sampling rate; `orientation.py` changes only the output transform after a stable reading. Sway retains `0 1 0 1 0 0` as its fixed touch calibration and maps touches through output rotation. All four directions passed the owner’s physical check, and the settings survived reboot. The two orientation services run without Wi-Fi or internal-storage access.
