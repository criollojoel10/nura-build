#!/bin/sh
# Power on the MediaTek connac Wi-Fi radio without udev or the vendor HAL.
#
# The gen4m driver is built into the kernel and probes at boot, but no netdev
# appears until userspace feeds it the per-device NVRAM through /dev/wmtWifi
# and then asserts the power GPIO with "echo 1".  On Android wmt_loader does
# this before conninfra starts; here we do it ourselves.
#
# Requirements, both proven in probe7.log:
#   * the driver parses the command from the first bytes of a *single* write()
#     and hands the rest of that same write to the NVRAM handler.  `printf`
#     followed by `cat` splits it in two and the handler gets length 0.
#   * /lib/firmware/wlan_6877_axi.nvram must exist before the write, otherwise
#     the callbacks are never registered and "echo 1" dies with
#     "mtk_wland_thread_main[E]: Invalid pointer".
#
# Idempotent: a second run on an already powered radio is a no-op.
set -u

FW=/lib/firmware/wlan_6877_axi.nvram
TMP=/run/nura-wifi-nvram.bin

log() {
    echo "nura-wifi: $*" > /dev/kmsg 2>/dev/null
    echo "nura-wifi: $*"
}

die() {
    log "ERROR: $*"
    exit 1
}

[ -r "$FW" ] || die "missing $FW; firmware bundle not installed"
[ -d /sys/class/net ] || die "no /sys/class/net"

# Already up? Leave the radio alone.
if [ -e /sys/class/net/wlan0 ]; then
    log "wlan0 already present"
    ip link set wlan0 up 2>/dev/null
    exit 0
fi

major_of() {
    awk -v name="$1" '$2 == name { print $1; exit }' /proc/devices
}

WMT_MAJOR=$(major_of wmtWifi)
[ -n "$WMT_MAJOR" ] || WMT_MAJOR=499
STPBT_MAJOR=$(major_of stpbt)
[ -n "$STPBT_MAJOR" ] || STPBT_MAJOR=192
log "wmtWifi major=$WMT_MAJOR stpbt major=$STPBT_MAJOR"

[ -e /dev/wmtWifi ] || mknod -m 660 /dev/wmtWifi c "$WMT_MAJOR" 0 || die "mknod /dev/wmtWifi"
[ -e /dev/stpbt ] || mknod -m 660 /dev/stpbt c "$STPBT_MAJOR" 0 || die "mknod /dev/stpbt"

# One shot buffer: "WR-BUF:NVRAM" + raw NVRAM, written in a single write(2).
{ printf 'WR-BUF:NVRAM'; cat "$FW"; } > "$TMP" || die "cannot build $TMP"
size=$(wc -c < "$TMP")
dd if="$TMP" of=/dev/wmtWifi bs=8192 count=1 2>/dev/null || die "NVRAM write failed"
log "nvram written size=$size"

sleep 1
# The first power-on can fail with EIO while connsys is still coming up
# ("wlan_func_on_by_chrdev: Timeout, pre_cal_flow: 0").  Retry a few times.
i=0
while [ "$i" -lt 3 ]; do
    if echo 1 > /dev/wmtWifi 2>/dev/null; then
        log "power on ok (try $((i + 1)))"
        j=0
        while [ "$j" -lt 15 ]; do
            [ -e /sys/class/net/wlan0 ] && break
            sleep 1
            j=$((j + 1))
        done
        [ -e /sys/class/net/wlan0 ] && break
    else
        log "power on failed (try $((i + 1)))"
    fi
    i=$((i + 1))
    sleep 5
done
[ -e /sys/class/net/wlan0 ] || die "wlan0 never appeared after power on"

ip link set wlan0 up || die "ip link set wlan0 up"
log "wlan0 up after ${i}s"
exit 0
