#!/usr/bin/env python3
"""Print sanitized, read-only macOS USB inventory without contacting devices."""
import datetime
import json
import plistlib
import shutil
import subprocess
import sys


def run(command):
    result = subprocess.run(command, capture_output=True, text=True, timeout=45)
    if result.returncode:
        raise RuntimeError("Host inventory command failed: " + command[0])
    return result.stdout


def registry_nodes(class_name):
    raw = run(["/usr/sbin/ioreg", "-r", "-c", class_name, "-a"])
    return plistlib.loads(raw.encode()) if raw.strip() else []


def walk(nodes):
    for node in nodes:
        yield node
        yield from walk(node.get("IORegistryEntryChildren", []))


def descriptors(nodes, fields):
    # Explicit allowlist: never publish serials, registry IDs, names or paths.
    return [
        {field: node[field] for field in fields
         if field in node and isinstance(node[field], int)}
        for node in nodes
    ]


def inspect():
    supported = run(["/usr/sbin/system_profiler", "-listDataTypes"])
    datatype = ("SPUSBHostDataType" if "SPUSBHostDataType" in supported
                else "SPUSBDataType")
    inventory = json.loads(run(["/usr/sbin/system_profiler", datatype, "-json"]))
    buses = inventory.get(datatype, [])
    devices = registry_nodes("IOUSBHostDevice")
    interfaces = registry_nodes("IOUSBHostInterface")
    device_nodes = [node for node in walk(devices) if "idVendor" in node]
    interface_nodes = [node for node in walk(interfaces)
                       if "bInterfaceClass" in node]
    return {
        "captured_at_utc": datetime.datetime.now(
            datetime.timezone.utc).isoformat(),
        "macos_version": run(["/usr/bin/sw_vers", "-productVersion"]).strip(),
        "macos_build": run(["/usr/bin/sw_vers", "-buildVersion"]).strip(),
        "system_profiler_datatype": datatype,
        "usb_bus_count": len(buses),
        "system_profiler_attached_entries": sum(
            len(bus.get("_items", [])) for bus in buses),
        "usb_device_count": len(device_nodes),
        "usb_interface_count": len(interface_nodes),
        "usb_device_descriptors": descriptors(device_nodes, [
            "idVendor", "idProduct", "bcdDevice", "bcdUSB",
            "bDeviceClass", "bDeviceSubClass", "bDeviceProtocol",
        ]),
        "usb_interface_descriptors": descriptors(interface_nodes, [
            "bInterfaceClass", "bInterfaceSubClass", "bInterfaceProtocol",
        ]),
        "tools_available_on_path": {
            tool: shutil.which(tool) is not None for tool in
            ["adb", "fastboot", "rkdeveloptool", "heimdall", "lsusb"]
        },
    }


if __name__ == "__main__":
    if sys.platform != "darwin":
        sys.exit("This diagnostic requires macOS.")
    try:
        print(json.dumps(inspect(), indent=2))
    except (OSError, RuntimeError, subprocess.TimeoutExpired, ValueError):
        sys.exit("Could not read the Mac USB inventory; no device commands issued.")
