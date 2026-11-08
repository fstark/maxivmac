#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Part of maxivmac (GPL v2, see LICENSE).
"""Encode a macOS file (data fork + ..namedfork/rsrc + com.apple.FinderInfo) as MacBinary II.
usage: python3 -I macbinary.py INPUT OUTPUT.bin [--name NAME] [--type TTTT --creator CCCC]
Type/creator/flags come from com.apple.FinderInfo (via `xattr -px`) unless overridden.
The Finder 'inited' flag (0x0100) and the file location are cleared so the Finder places the icon itself.
"""
import os, struct, subprocess, sys

def crc16(data, crc=0):
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc

def finderinfo(path):
    out = subprocess.run(['xattr', '-px', 'com.apple.FinderInfo', path],
                         capture_output=True, text=True, check=True).stdout
    return bytes.fromhex(out.replace('\n', '').replace(' ', ''))

def encode(path, name=None, ftype=None, creator=None):
    data = open(path, 'rb').read()
    try:
        rsrc = open(path + '/..namedfork/rsrc', 'rb').read()
    except OSError:
        rsrc = b''
    fi = finderinfo(path)
    t, c = fi[0:4], fi[4:8]
    flags = struct.unpack('>H', fi[8:10])[0] & ~0x0100
    if ftype: t = ftype.encode('mac_roman')
    if creator: c = creator.encode('mac_roman')
    name = name or os.path.basename(path)
    nb = name.encode('mac_roman')[:63]
    h = bytearray(128)
    h[1] = len(nb); h[2:2 + len(nb)] = nb
    h[65:69] = t; h[69:73] = c
    h[73] = flags >> 8; h[101] = flags & 0xFF
    h[83:87] = struct.pack('>I', len(data))
    h[87:91] = struct.pack('>I', len(rsrc))
    st = os.stat(path)
    mt = int(st.st_mtime) + 2082844800
    h[91:95] = struct.pack('>I', mt); h[95:99] = struct.pack('>I', mt)
    h[122] = 129; h[123] = 129
    h[124:126] = struct.pack('>H', crc16(bytes(h[:124])))
    pad = lambda b: b + b'\0' * (-len(b) % 128)
    return bytes(h) + pad(data) + pad(rsrc), (t, c, len(data), len(rsrc))

if __name__ == '__main__':
    a = sys.argv[1:]
    opt = {}
    pos = []
    i = 0
    while i < len(a):
        if a[i].startswith('--'): opt[a[i][2:]] = a[i + 1]; i += 2
        else: pos.append(a[i]); i += 1
    blob, info = encode(pos[0], opt.get('name'), opt.get('type'), opt.get('creator'))
    open(pos[1], 'wb').write(blob)
    print('%s: type=%r creator=%r data=%d rsrc=%d' % ((pos[0],) + info))
