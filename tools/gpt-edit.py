#!/usr/bin/env python3
"""Edita la GPT para dividir userdata y crear `linux` (ruby, sector 4096).

Modos:
  gpt-edit.py edit DEV PART_START NEW_END LINUX_START LINUX_END BACKUP
  gpt-edit.py verify DEV
  gpt-edit.py restore DEV BACKUP

Escribe tabla primaria y de respaldo con sus CRC32, preservando el type y el
unique GUID de userdata. El backup guarda cabecera primaria (92 B) + entradas.
"""
import fcntl
import os
import struct
import sys
import uuid
import zlib

BLKGETSIZE64 = 0x80081272
LINUX_TYPE = uuid.UUID('0FC63DAF-8483-4772-8E79-3D69D8477DE4').bytes_le
NENT, ESZ = 128, 128


def dev_size(fd):
    return struct.unpack('Q', fcntl.ioctl(fd, BLKGETSIZE64, b'\0' * 8))[0]


def ss_of(dev):
    p = '/sys/block/%s/queue/logical_block_size' % os.path.basename(dev)
    return int(open(p).read())


def read_gpt(dev):
    ss = ss_of(dev)
    f = open(dev, 'rb')
    f.seek(ss)
    hdr = f.read(92)
    assert hdr[:8] == b'EFI PART', 'no hay GPT'
    pte_lba = struct.unpack_from('<Q', hdr, 72)[0]
    f.seek(pte_lba * ss)
    ents = bytearray(f.read(NENT * ESZ))
    f.close()
    return ss, hdr, ents


def crc_header(hdr):
    h = bytearray(hdr[:92])
    struct.pack_into('<I', h, 16, 0)
    return zlib.crc32(bytes(h)) & 0xffffffff


def write_gpt(dev, hdr, ents):
    ss = ss_of(dev)
    fd = os.open(dev, os.O_RDWR)
    total_lba = dev_size(fd) // ss
    blba = total_lba - 1
    ent_lba = blba - (NENT * ESZ) // ss

    h = bytearray(hdr[:92])
    struct.pack_into('<I', h, 16, 0)
    struct.pack_into('<I', h, 16, zlib.crc32(bytes(h)) & 0xffffffff)
    os.pwrite(fd, ents, 2 * ss)
    os.pwrite(fd, h, ss)

    bh = bytearray(h)
    struct.pack_into('<Q', bh, 24, blba)
    struct.pack_into('<Q', bh, 32, 1)
    struct.pack_into('<Q', bh, 72, ent_lba)
    struct.pack_into('<I', bh, 16, 0)
    struct.pack_into('<I', bh, 16, zlib.crc32(bytes(bh)) & 0xffffffff)
    os.pwrite(fd, ents, ent_lba * ss)
    os.pwrite(fd, bh, blba * ss)
    os.fsync(fd)
    os.close(fd)


def find(ents, name):
    for i in range(NENT):
        e = ents[i * ESZ:(i + 1) * ESZ]
        if e[0:16] == b'\0' * 16:
            continue
        nm = e[56:128].decode('utf-16le', 'ignore').rstrip('\0')
        if nm == name:
            return i
    return None


def dump(ents):
    for i in range(NENT):
        e = ents[i * ESZ:(i + 1) * ESZ]
        if e[0:16] == b'\0' * 16:
            continue
        s, en = struct.unpack_from('<QQ', e, 32)
        nm = e[56:128].decode('utf-16le', 'ignore').rstrip('\0')
        if nm in ('userdata', 'linux', 'flashinfo'):
            print('%d %s %d..%d (%d)' % (i + 1, nm, s, en, en - s + 1))


def main():
    a = sys.argv[1:]
    if a[0] == 'verify':
        ss, hdr, ents = read_gpt(a[1])
        assert crc_header(hdr) == struct.unpack_from('<I', hdr, 16)[0], 'CRC cabecera'
        dump(ents)
        return
    if a[0] == 'restore':
        dev, bak = a[1], a[2]
        data = open(bak, 'rb').read()
        hdr, ents = data[:92], bytearray(data[92:92 + NENT * ESZ])
        write_gpt(dev, hdr, ents)
        print('restaurado')
        return
    _, dev, start, new_end, lstart, lend, bak = a
    ss, hdr, ents = read_gpt(dev)
    open(bak, 'wb').write(hdr + bytes(ents))
    i = find(ents, 'userdata')
    assert i is not None, 'sin userdata'
    e = bytearray(ents[i * ESZ:(i + 1) * ESZ])
    s, _ = struct.unpack_from('<QQ', e, 32)
    assert s == int(start), 'start %d != %s' % (s, start)
    struct.pack_into('<Q', e, 40, int(new_end))
    ents[i * ESZ:(i + 1) * ESZ] = e
    slot = None
    for j in range(NENT):
        if ents[j * ESZ:(j + 1) * ESZ][0:16] == b'\0' * 16:
            slot = j
            break
    assert slot is not None, 'sin slot libre'
    ne = bytearray(ESZ)
    ne[0:16] = LINUX_TYPE
    ne[16:32] = uuid.uuid4().bytes_le
    struct.pack_into('<QQ', ne, 32, int(lstart), int(lend))
    nm = 'linux'.encode('utf-16le')
    ne[56:56 + len(nm)] = nm
    ents[slot * ESZ:(slot + 1) * ESZ] = ne
    write_gpt(dev, hdr, ents)
    # verificacion leyendo de nuevo
    _, _, chk = read_gpt(dev)
    iu = find(chk, 'userdata')
    il = find(chk, 'linux')
    assert iu is not None and il is not None, 'no se leen las entradas'
    su, eu = struct.unpack_from('<QQ', chk, iu * ESZ + 32)
    sl, el = struct.unpack_from('<QQ', chk, il * ESZ + 32)
    assert eu == int(new_end) and sl == int(lstart) and el == int(lend), 'verificacion fallida'
    print('ok userdata %d..%d linux %d..%d' % (su, eu, sl, el))


main()
