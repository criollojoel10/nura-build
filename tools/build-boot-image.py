#!/usr/bin/env python3
"""Assemble an Android boot image (header v2, MTK page 2048) for ruby.

Measured reference: boot_b of the device (header v2, page_size 2048,
kernel_addr 0x40080000, ramdisk_addr 0x51100000, tags_addr 0x47c80000,
no second stage, no dtb in boot; dtbo lives in its own partition).

Usage:
  build-boot-image.py --kernel Image.gz --ramdisk initramfs.cpio.gz -o boot.img
                      [--cmdline "..."] [--header config/boot-header.json]

Never writes to a device.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import struct
import sys

PAGE_DEFAULT = 2048
HEADER_V2_SIZE = 1660


def pad(data: bytes, page: int) -> bytes:
    rem = len(data) % page
    return data if rem == 0 else data + b"\x00" * (page - rem)


def update_sha(sha, blob: bytes | None) -> None:
    if blob is not None:
        sha.update(blob)
        sha.update(struct.pack("<I", len(blob)))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--kernel", required=True)
    ap.add_argument("--ramdisk", required=True)
    ap.add_argument("--second", default=None, help="optional second stage")
    ap.add_argument("--dtb", default=None, help="optional DTB blob (boot v2 dtb field)")
    ap.add_argument("--dtb-addr", default=None, help="dtb load address (default from header JSON)")
    ap.add_argument("--header", default=None, help="JSON with addresses/cmdline")
    ap.add_argument("--cmdline", default=None)
    ap.add_argument("--name", default="")
    ap.add_argument("--os-version", default=None, help="a.b.c or header JSON value")
    ap.add_argument("--out", "-o", required=True)
    args = ap.parse_args()

    cfg = {
        "page_size": PAGE_DEFAULT,
        "header_version": 2,
        "kernel_addr": "0x40080000",
        "ramdisk_addr": "0x51100000",
        "second_addr": "0x00000000",
        "tags_addr": "0x47c80000",
        "dtb_addr": "0x47c80000",
        "os_version": "17.0.426",
        "cmdline": "",
        "extra_cmdline": "",
        "name": "",
    }
    if args.header:
        with open(args.header, "r", encoding="utf-8") as fh:
            cfg.update(json.load(fh))

    page = int(cfg["page_size"])
    hdr_version = int(cfg["header_version"])
    if page not in (2048, 4096):
        raise SystemExit(f"unsupported page size {page}")
    if hdr_version != 2:
        raise SystemExit(f"ruby measured header v2, got v{hdr_version}")

    kernel = open(args.kernel, "rb").read()
    ramdisk = open(args.ramdisk, "rb").read()
    second = open(args.second, "rb").read() if args.second else b""
    dtb = open(args.dtb, "rb").read() if args.dtb else b""

    cmdline = (args.cmdline if args.cmdline is not None else cfg.get("cmdline", "")).encode()
    extra = cfg.get("extra_cmdline", "").encode()
    name = (args.name or cfg.get("name", "")).encode()
    if len(cmdline) > 511:
        raise SystemExit("cmdline too long for boot v2")
    if len(extra) > 1023:
        raise SystemExit("extra_cmdline too long")

    def os_version(value: str) -> int:
        value = str(value)
        if value.isdigit():
            return int(value)  # raw field, as measured on the device
        try:
            a, b, c = (int(x) for x in value.split("."))
        except ValueError:
            raise SystemExit(f"bad os_version {value!r} (want a.b.c or raw integer)")
        return (a << 25) | (b << 18) | (c << 11)

    kernel_addr = int(str(cfg["kernel_addr"]), 16)
    ramdisk_addr = int(str(cfg["ramdisk_addr"]), 16)
    second_addr = int(str(cfg["second_addr"]), 16)
    tags_addr = int(str(cfg["tags_addr"]), 16)
    os_ver = os_version(args.os_version or cfg["os_version"])

    sha = hashlib.sha1()
    update_sha(sha, kernel)
    update_sha(sha, ramdisk)
    update_sha(sha, second if second else None)
    update_sha(sha, dtb if dtb else None)
    img_id = bytes.fromhex(sha.hexdigest())

    hdr = bytearray(HEADER_V2_SIZE)
    struct.pack_into("<8s", hdr, 0, b"ANDROID!")
    struct.pack_into("<I", hdr, 8, len(kernel))
    struct.pack_into("<I", hdr, 12, kernel_addr)
    struct.pack_into("<I", hdr, 16, len(ramdisk))
    struct.pack_into("<I", hdr, 20, ramdisk_addr)
    struct.pack_into("<I", hdr, 24, len(second))
    struct.pack_into("<I", hdr, 28, second_addr)
    struct.pack_into("<I", hdr, 32, tags_addr)
    struct.pack_into("<I", hdr, 36, page)
    struct.pack_into("<I", hdr, 40, hdr_version)
    struct.pack_into("<I", hdr, 44, os_ver)
    struct.pack_into("<16s", hdr, 48, name[:16])
    struct.pack_into("<512s", hdr, 64, cmdline)
    struct.pack_into("<20s", hdr, 576, img_id)
    struct.pack_into("<1024s", hdr, 608, extra)
    # v2: recovery_dtbo_size @1632, recovery_dtbo_offset @1636, header_size @1644
    struct.pack_into("<I", hdr, 1632, 0)
    struct.pack_into("<Q", hdr, 1636, 0)
    struct.pack_into("<I", hdr, 1644, HEADER_V2_SIZE)
    # v2: dtb_size @1648, dtb_addr @1652 (measured on boot_b: 179139 @ 0x47c80000)
    struct.pack_into("<I", hdr, 1648, len(dtb))
    struct.pack_into("<Q", hdr, 1652, int(str(args.dtb_addr or cfg["dtb_addr"]), 16))

    out = pad(bytes(hdr), page) + pad(kernel, page) + pad(ramdisk, page)
    if second:
        out += pad(second, page)
    if dtb:
        out += pad(dtb, page)

    total = len(out)
    part = int(cfg.get("partition_size", 0) or 0)
    with open(args.out, "wb") as fh:
        fh.write(out)

    print(f"wrote {args.out}: {total} bytes "
          f"(hdr v{hdr_version}, page {page}, kernel {len(kernel)}, "
          f"ramdisk {len(ramdisk)}, second {len(second)}, dtb {len(dtb)})")
    if part and total > part:
        print(f"ERROR: image {total} > partition {part}", file=sys.stderr)
        return 2
    print(f"sha256 {hashlib.sha256(out).hexdigest()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
