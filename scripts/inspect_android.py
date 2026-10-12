#!/usr/bin/env python3
"""Read selected, non-unique Android facts from one existing ADB transport."""
import argparse
import datetime
import json
import re
import shutil
import subprocess
import sys

PROPERTIES = [
    "ro.product.manufacturer", "ro.product.brand", "ro.product.model",
    "ro.product.device", "ro.product.board", "ro.hardware", "ro.board.platform",
    "ro.build.display.id", "ro.build.fingerprint", "ro.build.version.release",
    "ro.build.version.sdk", "ro.build.version.security_patch",
    "ro.boot.verifiedbootstate", "ro.boot.flash.locked", "ro.secure",
    "ro.debuggable", "ro.adb.secure",
]


def adb_read(adb, serial, args):
    result = subprocess.run([adb, "-s", serial] + args,
                            capture_output=True, text=True, timeout=30)
    if result.returncode:
        # Errors can contain the transport serial. Do not echo raw stderr.
        raise RuntimeError("Selected ADB transport is unavailable or unauthorized.")
    return result.stdout.strip()


def inspect(adb, serial):
    if adb_read(adb, serial, ["get-state"]) != "device":
        raise RuntimeError("Selected transport is not in device state.")
    uid_text = adb_read(adb, serial, ["shell", "id"])
    uid = re.search(r"(?:^|\s)uid=(\d+)", uid_text)
    cpu = adb_read(adb, serial, ["shell", "cat", "/proc/cpuinfo"])
    allowed_cpu = {"Hardware", "Processor", "CPU architecture", "CPU implementer",
                   "CPU part", "Features"}
    cpu_fields = {}
    for line in cpu.splitlines():
        key, separator, value = line.partition(":")
        if separator and key.strip() in allowed_cpu:
            cpu_fields[key.strip()] = value.strip()
    parts = adb_read(adb, serial, ["shell", "cat", "/proc/partitions"])
    partitions = []
    for line in parts.splitlines():
        match = re.fullmatch(r"\s*(\d+)\s+(\d+)\s+(\d+)\s+([a-zA-Z0-9_-]+)\s*", line)
        if match:
            partitions.append({"name": match.group(4),
                               "blocks": int(match.group(3))})
    return {
        "captured_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "transport_state": "device",
        "shell_uid": int(uid.group(1)) if uid else None,
        "properties": {key: adb_read(adb, serial, ["shell", "getprop", key])
                       for key in PROPERTIES},
        "kernel": adb_read(adb, serial, ["shell", "uname", "-sr"]),
        "cpu_fields": cpu_fields,
        "partitions": partitions,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True,
                        help="Existing, locally verified frame transport; never printed")
    parser.add_argument("--adb", default=shutil.which("adb"),
                        help="Path to already installed/approved adb")
    args = parser.parse_args()
    if not args.adb:
        parser.error("ADB is not installed. Obtain approval for official Platform-Tools.")
    try:
        print(json.dumps(inspect(args.adb, args.serial), indent=2))
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        if isinstance(error, RuntimeError):
            sys.exit(str(error))
        sys.exit("Read-only Android inspection failed; raw device details omitted.")


if __name__ == "__main__":
    main()
