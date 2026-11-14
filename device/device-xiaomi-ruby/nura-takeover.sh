#!/bin/sh
# Take the DRM card back from the initramfs nurafb.
#
# stage 1 starts nurafb before switch_root; that process survives the switch
# and keeps DRM master, so the rootfs copy gets EACCES on SETCRTC.  Kill it by
# comm (its exe lives in the deleted initramfs root) and wait until the PID is
# really gone before nura-console starts.
#
# Everything is logged to /dev/kmsg so it shows on the panel even if the
# rootfs journal is rotated or missing.
log() {
    echo "nura-takeover: $*" > /dev/kmsg 2>/dev/null
    echo "nura-takeover: $*" >> /run/nura-takeover.log 2>/dev/null
}
mkdir -p /run 2>/dev/null

# Ask nurafb to leave on its own first: its exit path disables the CRTC
# before freeing the framebuffer. Killing it with SIGTERM skips that and leaves
# the OVL reading an unmapped IOVA (mtk-iommu fault, pa=0).
: > /run/nurafb.stop 2>/dev/null
i=0
while [ "$i" -lt 20 ]; do
    pgrep -x nurafb >/dev/null 2>&1 || break
    sleep 0.1
    i=$((i + 1))
done

victims=
for d in /proc/[0-9]*; do
    [ -r "$d/comm" ] || continue
    [ "$(cat "$d/comm" 2>/dev/null)" = nurafb ] || continue
    pid=${d#/proc/}
    log "killing initramfs nurafb pid $pid"
    kill -TERM "$pid" 2>/dev/null
    victims="$victims $pid"
done
[ -n "$victims" ] || log "no nurafb process found"

for pid in $victims; do
    i=0
    while [ -d "/proc/$pid" ] && [ "$i" -lt 50 ]; do
        sleep 0.1
        i=$((i + 1))
    done
    if [ -d "/proc/$pid" ]; then
        log "pid $pid ignored TERM, sending KILL"
        kill -KILL "$pid" 2>/dev/null
        sleep 0.3
    else
        log "pid $pid exited"
    fi
done

# Diagnostics: anything else holding card0 would also block master takeover.
for d in /proc/[0-9]*; do
    [ -d "$d/fd" ] || continue
    for f in "$d"/fd/*; do
        [ -e "$f" ] || continue
        [ "$(readlink "$f" 2>/dev/null)" = /dev/dri/card0 ] || continue
        log "card0 still open by ${d#/proc/} ($(cat "$d/comm" 2>/dev/null))"
    done
done

exit 0
