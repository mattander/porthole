#!/usr/bin/env python3
"""Bounded USB transport restricted to the matched SD Linux frame."""
import os,plistlib,re,select,subprocess,time

def frame_port(registry):
    # Follow the matched USB device's children rather than publishing/pinning a
    # serial number or choosing an unrelated modem elsewhere in the registry.
    def nodes(value):
        if isinstance(value,list):
            for child in value:yield from nodes(child)
        elif isinstance(value,dict):
            yield value
            yield from nodes(value.get('IORegistryEntryChildren',[]))
    matched=[node for node in nodes(registry) if node.get('IOObjectClass')=='IOUSBHostDevice'
             and node.get('idVendor')==7531 and node.get('idProduct')==260
             and node.get('USB Product Name')=='PF1007L Linux']
    ports=[node['IOCalloutDevice'] for device in matched for node in nodes(device)
           if isinstance(node.get('IOCalloutDevice'),str)
           and re.fullmatch(r'/dev/cu\.usbmodem[A-Za-z0-9_-]+',node['IOCalloutDevice'])]
    if len(matched)!=1 or len(ports)!=1:raise RuntimeError('Connect exactly one PF1007L frame running SD Linux over USB.')
    return ports[0]

def find_frame():
    raw=subprocess.check_output(['/usr/sbin/ioreg','-p','IOService','-r','-c','IOUSBHostDevice','-a','-l'])
    return frame_port(plistlib.loads(raw))

def send(fd,data):
    deadline=time.monotonic()+5;offset=0
    while offset<len(data):
        if time.monotonic()>deadline:raise RuntimeError('USB write timed out')
        if select.select([],[fd],[],.1)[1]:
            try:offset+=os.write(fd,data[offset:offset+128])
            except BlockingIOError:pass

def wait_marker(fd,marker,seconds):
    # Do not retain or print console output: it can contain unrelated private data.
    deadline=time.monotonic()+seconds;tail=b''
    while time.monotonic()<deadline:
        if select.select([fd],[],[],.2)[0]:
            try:data=os.read(fd,4096)
            except BlockingIOError:continue
            if not data:break
            tail=(tail+data)[-8192:]
            if marker in tail:return
    raise RuntimeError('Frame setup handshake timed out; key status is unconfirmed.')
