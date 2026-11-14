#!/usr/bin/env python3
"""Imprime la cabecera de un boot.img / vendor_boot.img / dtbo.img de Android.

Soporta boot header v0-v2 (page-based) y v3/v4 (4096 fijo), vendor_boot v3/v4
y dtbo (magic d7b7ab1e). Lee solo cabecera y blobs declarados.
"""
import struct, sys, hashlib


def align(n, p=4096):
    return (n + p - 1) // p * p


def sha_region(f, off, size):
    if not size:
        return "-"
    f.seek(off)
    h = hashlib.sha256()
    left = size
    while left:
        chunk = f.read(min(left, 1 << 20))
        if not chunk:
            break
        h.update(chunk)
        left -= len(chunk)
    return h.hexdigest()[:16]


def parse_boot(f, data):
    if data[:8] != b"ANDROID!":
        return None
    hver = struct.unpack_from("<I", data, 40)[0]
    if hver in (0, 1, 2):
        (ksize, kaddr, rsize, raddr, ssize, saddr, tags,
         page, _hv, osver) = struct.unpack_from("<IIIIIIIIII", data, 8)
        name = data[48:64].split(b"\0")[0].decode("utf-8", "replace")
        cmdline = data[64:576].split(b"\0")[0].decode("utf-8", "replace")
        extra = data[608:1632].split(b"\0")[0].decode("utf-8", "replace")
        off = page
        sha = {}
        for key, size in (("kernel", ksize), ("ramdisk", rsize), ("second", ssize)):
            if size:
                sha[key] = sha_region(f, off, size)
                off = align(off + size, page)
        return {
            "magic": "ANDROID!", "header_version": hver, "page_size": page,
            "kernel_size": ksize, "kernel_addr": hex(kaddr),
            "ramdisk_size": rsize, "ramdisk_addr": hex(raddr),
            "second_size": ssize, "second_addr": hex(saddr),
            "tags_addr": hex(tags), "name": name,
            "os_version": f"{osver >> 25}.{(osver >> 18) & 0x7f}.{osver & 0x7fff}",
            "cmdline": cmdline, "extra_cmdline": extra, "sha256": sha,
        }

    (ksize, rsize, osver, hsize, _r0, _r1, _r2, _r3,
     _hv, patch) = struct.unpack_from("<IIIIIIIIII", data, 8)
    cmdline = data[44:44 + 1536].split(b"\0")[0].decode("utf-8", "replace")
    sig = struct.unpack_from("<I", data, 1580)[0] if hver >= 4 else 0
    off = 4096
    ksha = sha_region(f, off, ksize)
    off = align(off + ksize)
    rsha = sha_region(f, off, rsize)
    return {
        "magic": "ANDROID!", "header_version": hver, "page_size": 4096,
        "header_size": hsize, "kernel_size": ksize, "ramdisk_size": rsize,
        "signature_size": sig,
        "os_version": f"{osver >> 25}.{(osver >> 18) & 0x7f}.{osver & 0x7fff}",
        "os_patch_level": f"{1970 + (patch >> 4)}-{patch & 0xf:02d}",
        "cmdline": cmdline, "sha256": {"kernel": ksha, "ramdisk": rsha},
    }


def parse_vendor_boot(f, data):
    if data[:8] != b"VNDRBOOT":
        return None
    hver, page, kaddr, raddr, rsize, cmdline, tags, dsize, daddr = \
        struct.unpack_from("<IIIIII2048sIII", data, 8)
    # v4 añade vendor_ramdisk_table_size/entry_num/entry_size/bootconfig_size
    extra = {}
    if hver >= 4:
        tbl, num, esz, bcfg = struct.unpack_from("<IIII", data, 2096)
        extra = {"vendor_ramdisk_table_size": tbl, "vendor_ramdisk_table_entry_num": num,
                 "vendor_ramdisk_table_entry_size": esz, "bootconfig_size": bcfg}
    info = {
        "magic": "VNDRBOOT", "header_version": hver, "page_size": page,
        "kernel_addr": hex(kaddr), "ramdisk_addr": hex(raddr),
        "vendor_ramdisk_size": rsize, "dtb_size": dsize, "dtb_addr": hex(daddr),
        "cmdline": cmdline.split(b"\0")[0].decode("utf-8", "replace"),
    }
    info.update(extra)
    return info


def parse_dtbo(f, data):
    if data[:4] != b"\xd7\xb7\xab\x1e":
        return None
    total, hsize, esize, ecount, eoff, page, version = \
        struct.unpack_from(">IIIIIII", data, 4)
    return {
        "magic": "d7b7ab1e", "version": version, "total_size": total,
        "header_size": hsize, "entry_size": esize,
        "entry_count": ecount, "entries_offset": eoff, "page_size": page,
    }


def main(path):
    with open(path, "rb") as f:
        head = f.read(1 << 16)
        for fn in (parse_boot, parse_vendor_boot, parse_dtbo):
            info = fn(f, head)
            if info:
                break
        else:
            print(f"{path}: cabecera desconocida {head[:8]!r}")
            return 1
    print(f"== {path}")
    for k, v in info.items():
        print(f"  {k}: {v}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
