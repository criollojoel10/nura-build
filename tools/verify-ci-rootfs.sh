#!/bin/bash
# Verify a Nura rootfs image (CI artifact or live partition) *before* risking
# the working rootfs on the phone.
#
# The CI build is only useful if the artifact really is the environment that
# made Plasma visible: same plasma-mobile drop-ins (no VT, software KWin,
# backlight re-send), the /usr/local/sbin helpers, Mesa, NetworkManager, sshd.
# Checking that offline is cheap; discovering it after `dd` is not.
#
# Usage:
#   tools/verify-ci-rootfs.sh <rootfs.img[.zst] | directory | block device>
#
# With .zst it decompresses to $TMPDIR first. Mounting needs root (su -c on
# Android). Exit 0 only if every check passes.
set -Eeuo pipefail

TARGET=${1:?uso: $0 <imagen | directorio | dispositivo>}
# En el telefono el temporal vive en Termux; en un runner de GitHub no existe.
TMP=${TMPDIR:-}
if [ -z "$TMP" ]; then
    if [ -d /data/data/com.termux/files/usr/tmp/opencode ]; then
        TMP=/data/data/com.termux/files/usr/tmp/opencode
    else
        TMP=/tmp
    fi
fi
MNT=$(mktemp -d "${TMP}/verify-rootfs.XXXXXX")
IMG=
CLEANUP_IMG=0
CLEANUP_MNT=0

cleanup() {
    [ "$CLEANUP_MNT" = 1 ] && { umount "$MNT" 2>/dev/null || true; }
    rmdir "$MNT" 2>/dev/null || true
    [ "$CLEANUP_IMG" = 1 ] && rm -rf "$IMG"
    return 0
}
trap cleanup EXIT

as_root() {
    if [ "$(id -u)" = 0 ]; then
        "$@"
    else
        # El shell de `su` no hereda el PATH de Termux (mount/debugfs viven ahi).
        su -c "PATH=$PATH $(printf '%q ' "$@")"
    fi
}

case "$TARGET" in
    *.zst)
        IMG=$(mktemp "${TMP}/rootfs.XXXXXX.img")
        CLEANUP_IMG=1
        echo "== descomprimiendo $TARGET"
        zstd -dc "$TARGET" > "$IMG"
        TARGET=$IMG
        ;;
esac

MODE=
IMG_FILE=
if [ -d "$TARGET" ]; then
    MODE=dir; R=$TARGET
elif [ -e "$TARGET" ]; then
    echo "== montando $TARGET (ro)"
    if as_root mount -o loop,ro "$TARGET" "$MNT" 2>/dev/null && [ -e "$MNT/etc" ]; then
        MODE=mount; CLEANUP_MNT=1; R=$MNT
    else
        # Android no admite losetup desde la app (aunque seas root). debugfs
        # lee el ext4 sin montar, y en un runner sirve como plan B.
        command -v debugfs >/dev/null 2>&1 || {
            echo "no se puede montar y no hay debugfs" >&2; exit 1; }
        echo "== sin loop: leyendo el ext4 con debugfs"
        MODE=debugfs; IMG_FILE=$TARGET
    fi
else
    echo "no existe: $TARGET" >&2
    exit 1
fi

# Lee un fichero del rootfs a stdout, en cualquiera de los tres modos.
# /sbin -> usr/bin y /etc/os-release son enlaces (usr-merge), asi que en modo
# debugfs hay que resolverlos a mano: "Fast link dest: ...".
resolve_dbg() { # imprime la ruta final dentro del rootfs
    local path=$1 dest i=0
    while [ $i -lt 4 ]; do
        dest=$(debugfs -R "stat $path" "$IMG_FILE" 2>/dev/null \
               | sed -n 's/^Fast link dest: "\(.*\)"$/\1/p')
        [ -n "$dest" ] || break
        case "$dest" in
            /*) path=$dest ;;
            *)  path="$(dirname "$path")/$dest" ;;
        esac
        i=$((i + 1))
    done
    printf '%s\n' "$path"
}

read_file() { # read_file <ruta absoluta dentro del rootfs>
    case "$MODE" in
        debugfs) debugfs -R "cat $(resolve_dbg "$1")" "$IMG_FILE" 2>/dev/null ;;
        *)       cat "$R$1" 2>/dev/null ;;
    esac
}

file_exists() {
    local path=$1 d f n
    case "$MODE" in
        debugfs)
            case "$path" in
                *"*"*)   # '*' literal: '*"*"' a secas NO casa (comprobado)
                    # debugfs no expande globs; "ls -l" deja el nombre al final.
                    d=$(dirname "$path"); f=$(basename "$path")
                    for n in $(debugfs -R "ls -l $d" "$IMG_FILE" 2>/dev/null \
                               | awk '{print $NF}'); do
                        # shellcheck disable=SC2254
                        case "$n" in $f) return 0 ;; esac
                    done
                    return 1 ;;
            esac
            debugfs -R "stat $path" "$IMG_FILE" 2>/dev/null | grep -q 'Inode:' ;;
        *) [ -e "$R$path" ] ;;
    esac
}

file_sha() {
    local t
    case "$MODE" in
        debugfs)
            t=$(mktemp "${TMP}/dbg.XXXXXX")
            read_file "$1" > "$t"
            [ -s "$t" ] && sha256sum "$t" | cut -d' ' -f1
            rm -f "$t" ;;
        *) [ -f "$R$1" ] && sha256sum "$R$1" | cut -d' ' -f1 ;;
    esac
}

fail=0
ok=0
check_file() { # check_file <ruta[ con glob]> [patrón]
    local p=$1 pat=${2:-}
    if [ "$MODE" = debugfs ]; then
        if ! file_exists "$p"; then
            printf 'FALTA    %s\n' "$p"; fail=$((fail + 1)); return
        fi
        if [ -n "$pat" ] && ! read_file "$p" | grep -qaE "$pat"; then
            printf 'CONTENIDO %s no contiene /%s/\n' "$p" "$pat"; fail=$((fail + 1)); return
        fi
        printf 'ok       %s\n' "$p"; ok=$((ok + 1)); return
    fi
    # -L: /sbin y /bin son enlaces a /usr/bin (usr-merge), find no entra solo.
    if [ -z "$(find -L "$R$(dirname "$p")" -maxdepth 1 -name "$(basename "$p")" 2>/dev/null | head -1)" ]; then
        printf 'FALTA    %s\n' "$p"; fail=$((fail + 1)); return
    fi
    if [ -n "$pat" ] && ! grep -qaE "$pat" "$R$p" 2>/dev/null; then
        printf 'CONTENIDO %s no contiene /%s/\n' "$p" "$pat"; fail=$((fail + 1)); return
    fi
    printf 'ok       %s\n' "$p"; ok=$((ok + 1))
}

echo "== identidad"
check_file /etc/os-release 'PRETTY_NAME="Nura|postmarketOS'
check_file /usr/lib/systemd/systemd
check_file /etc/fstab

echo "== helpers Nura"
for f in nura-takeover nura-gpu-prep nura-backlight nura-shot nura-wifi \
         nura-net-report nura-gpu-probe nura-bars-test nura-dt-report nurafb; do
    check_file "/usr/bin/$f"
done

echo "== unidades y drop-ins"
check_file /etc/systemd/system/nura-wifi.service
check_file /etc/systemd/system/nura-net-report.service
check_file /etc/systemd/system/nura-gpu-probe.service
check_file /etc/systemd/system/plasma-mobile.service.d/10-nura-software.conf \
           'KWIN_DRM_NO_AMS=1'
check_file /etc/systemd/system/plasma-mobile.service.d/20-nura-notty.conf \
           'ConditionPathExists=/dev/dri/card0'

echo "== Plasma / red / GPU userspace"
check_file /usr/bin/plasmashell
check_file /usr/bin/kwin_wayland
check_file /usr/lib/libgallium-*.so
check_file /usr/bin/NetworkManager
check_file /usr/bin/sshd
check_file /usr/bin/python3

echo "== cuentas y arranque"
check_file /home/nura
check_file /etc/systemd/system/multi-user.target.wants/nura-wifi.service

# El drop-in debe ser idéntico al que funciona en el teléfono.
for f in 10-nura-software.conf 20-nura-notty.conf; do
    want=$(sha256sum "$(dirname "$0")/../pmaports/device/testing/device-xiaomi-ruby/$f" 2>/dev/null | cut -d' ' -f1)
    got=$(file_sha "/etc/systemd/system/plasma-mobile.service.d/$f")
    if [ -n "$want" ] && [ "$want" = "$got" ]; then
        printf 'ok       drop-in %s idéntico al del repo (%s)\n' "$f" "${got:0:12}"
        ok=$((ok + 1))
    else
        printf 'DIFIERE  drop-in %s repo=%s imagen=%s\n' "$f" "${want:0:12}" "${got:0:12}"
        fail=$((fail + 1))
    fi
done

echo
echo "== resumen: $ok OK, $fail fallos"
[ "$fail" -eq 0 ] || exit 1
