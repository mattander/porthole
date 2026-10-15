# Frame display controls

Settings → Display offers Manual, Auto (light sensor), and Auto (sunrise / sunset).
Light / Dark also returns to Manual. Existing `display/followSun` preferences
migrate to the sunrise mode; the legacy flag remains consistent for rollback.
The last theme and mode are saved in the GUI user's preferences, alongside the
manual brightness percentage. Brightness remains independent of theme mode.

The PF1007L panel exposes ten backlight steps. Settings provides a large slider
and minus/plus buttons, uses the panel's actual step count, verifies each write,
and never writes zero. Missing hardware or denied writes show an unavailable/error
state. A failed write does not replace the saved preference. Demo controls change
only the preview; they never read a sensor or write hardware/preferences.

## Backlight access bootstrap

The GUI stays unprivileged. A boot-time oneshot changes group/mode on exactly
`/sys/class/backlight/backlight/brightness` to `root:porthole 0660`. It does not
change brightness, expose a command socket, grant sudo, or grant access to other
sysfs controls. The unit is ordered after display startup and before the kiosk,
with its filesystem write scope restricted to that one attribute.

Install once through the already trusted USB maintenance console. Prepare a
reviewed bundle containing this directory's service and installer, with a
`files.sha256` for both. Run `install-display.sh` as root on the verified SD-only
system. It checks the pinned kernel, refuses replacement of an existing unit,
and restores the previous permission/group on an installation failure. Ordinary
signed app updates cannot install this root-owned unit: the existing migrator
still only accepts its fixed credential declarations.

## Light sensor status

The app reads processed Linux IIO illuminance inputs in lux using the
[kernel IIO ABI](https://github.com/torvalds/linux/blob/master/Documentation/ABI/testing/sysfs-bus-iio).
It uses a bounded asynchronous read so a stalled driver does not freeze touch
input. Raw ADC/intensity channels are not treated as lux. Dim means ≤15 lux and
bright means ≥40 lux; fifteen continuous seconds are required before switching.
The middle band holds the current theme. Missing, invalid, delayed, or failed
readings reset the candidate and keep the current theme.

On this particular frame, the October 15 live probe found thermal, AXP ADC and
SC7A20 accelerometer IIO devices but no illuminance input. Read-only LTR identity
register reads at the vendor-configured I2C1 address `0x23` failed. This does not
prove whether the chip is absent, unpowered, or needs another hardware binding.
Sensor mode therefore reports unavailable on this unit; it does not silently
substitute the clock or claim to sense light. Enabling a driver/power path needs
separate hardware investigation. No kernel, device tree, I2C writes or boot
configuration changes are part of these controls.
