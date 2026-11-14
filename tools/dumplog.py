#!/usr/bin/env python3
import sys, struct, lzma

def align8(n): return (n + 7) & ~7

def parse(path):
    b = open(path, 'rb').read()
    if b[:8] != b'LPKSHHRH':
        print('bad sig', path); return
    hdr_size, arena_size = struct.unpack_from('<QQ', b, 88)
    data = {}
    fields = {}
    entries = []
    pos = hdr_size
    end = hdr_size + arena_size
    while pos + 16 <= end:
        otype = b[pos]
        flags = b[pos+1]
        size = struct.unpack_from('<Q', b, pos+8)[0]
        if size < 16 or pos + size > len(b):
            break
        payload = b[pos+16:pos+size]
        if otype == 1:
            body = payload[40:]
            if flags & 1:
                try:
                    body = lzma.decompress(body)
                except Exception:
                    body = b''
            data[pos] = body
        elif otype == 2:
            fields[pos] = payload[16:].rstrip(b'\x00')
        elif otype == 3:
            entries.append(pos)
            fields.setdefault('_E', {})[pos] = payload
        pos += align8(size)
    out = []
    for e in entries:
        p = fields['_E'][e]
        mono = struct.unpack_from('<Q', p, 16)[0]
        nitems = (len(p) - 48) // 8
        arr = [struct.unpack_from('<Q', p, 48 + i*8)[0] for i in range(nitems)]
        kv = {}
        i = 0
        while i + 1 < len(arr):
            f = fields.get(arr[i]); d = data.get(arr[i+1])
            if f is not None:
                kv[f.decode('utf-8', 'replace')] = d.decode('utf-8', 'replace') if d else ''
                i += 2
            else:
                i += 1
        msg = kv.get('MESSAGE')
        if msg is None:
            continue
        unit = kv.get('_SYSTEMD_UNIT', '')
        ident = kv.get('SYSLOG_IDENTIFIER', '')
        out.append((mono, unit, ident, msg))
    out.sort()
    for mono, unit, ident, msg in out:
        print(f'[{mono/1e6:9.3f}] {unit or ident}: {msg}')

parse(sys.argv[1])
