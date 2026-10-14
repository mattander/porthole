#!/usr/bin/env python3
"""Rotate the verified PF1007L display; touch calibration belongs to Sway."""
import json
import logging
import subprocess
import time
from pathlib import Path


def classify(axes, config):
    x, y, z = axes
    gravity = max(abs(x), abs(y))
    if gravity < 600 or abs(z) >= gravity * 1.3:
        return None
    if abs(x) > abs(y) * 1.2:
        return config["x_positive" if x > 0 else "x_negative"]
    if abs(y) > abs(x) * 1.2:
        return config["y_positive" if y > 0 else "y_negative"]
    return None


def main():
    logging.basicConfig(level=logging.INFO, format="%(message)s")
    config = json.loads(Path("/etc/porthole/orientation.json").read_text())
    for key in ("x_positive", "x_negative", "y_positive", "y_negative"):
        if config[key] not in (0, 90, 180, 270):
            raise ValueError("Unsupported display orientation")
    sensor = next(p for p in Path("/sys/bus/iio/devices").glob("iio:device*")
                  if (p / "name").exists() and (p / "name").read_text().strip() == "sc7a20")
    if float((sensor / "sampling_frequency").read_text()) < 10:
        raise RuntimeError("Orientation sensor sampling rate is too slow")
    current = candidate = None
    since = last_error = 0.0
    while True:
        try:
            axes = [int((sensor / ("in_accel_" + a + "_raw")).read_text()) for a in "xyz"]
            angle = classify(axes, config)
            now = time.monotonic()
            if angle != candidate:
                candidate, since = angle, now
            if angle is not None and angle != current and now - since >= config.get("settle_seconds", 0.6):
                sock = next(Path("/run/porthole-kiosk").glob("sway-ipc.*.sock"))
                result = subprocess.run(["swaymsg", "-r", "-s", str(sock),
                                         "output DSI-1 transform " + str(angle)],
                                        capture_output=True, text=True, timeout=3)
                replies = json.loads(result.stdout)
                if result.returncode or not replies or not all(v.get("success") for v in replies):
                    raise RuntimeError("Display orientation command failed")
                current = angle
                state = {"transform": angle, "acceleration": axes}
                Path("/run/porthole-kiosk/orientation-state.json").write_text(json.dumps(state))
                logging.info("Display orientation: %s", angle)
        except (OSError, ValueError, StopIteration, subprocess.SubprocessError, RuntimeError) as error:
            if time.monotonic() - last_error >= 30:
                logging.warning("Orientation update failed: %s", type(error).__name__)
                last_error = time.monotonic()
        time.sleep(0.1)


if __name__ == "__main__":
    main()
