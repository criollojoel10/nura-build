#!/bin/bash
# Fail fast when a device package lists a source that the git tree does not
# actually have.
#
# Why: `nurafb.bin` matched `.gitignore` (*.bin), so `abuild` on a clean runner
# died with "sha512sum: can't open '/home/pmos/build/nurafb.bin'" *after*
# building the kernel and entering the aarch64 buildroot (~3 min per attempt).
# `aports` has no such safety net: it trusts `source="..."`.
#
# Usage: tools/check-port-sources.sh [dir of the APKBUILD]
set -Eeuo pipefail

HERE=$(cd "$(dirname "$0")/.." && pwd)
DIR=${1:-$HERE/pmaports/device/testing/device-xiaomi-ruby}
cd "$DIR"

[ -f APKBUILD ] || { echo "no APKBUILD in $DIR" >&2; exit 1; }

bash -n APKBUILD

# Read the variables with the same shell abuild uses.
eval "$(bash -c '. ./APKBUILD
    printf "SRCS=(%s)\n" "$source"
    printf "SUBPKGS=%q\n" "$subpackages"')"

[ "${#SRCS[@]}" -gt 0 ] || { echo "source= is empty" >&2; exit 1; }
[ -n "${SUBPKGS:-}" ] || { echo "subpackages= is empty (pbootstrap needs the kernel subpackage)" >&2; exit 1; }

fail=0
for f in "${SRCS[@]}"; do
    # Skip renamed sources (dest::url) and remote URLs.
    case "$f" in *::*) f=${f%%::*} ;; esac
    case "$f" in
        http://*|https://*|ftp://*)
            echo "REMOTO   $f"; continue ;;
    esac
    if [ ! -e "$f" ]; then
        echo "FALTA    $f (no está en el árbol de trabajo)"; fail=1
    elif ! git ls-files --error-unmatch "$f" >/dev/null 2>&1; then
        echo "NO-GIT   $f (existe pero no está versionado: el runner no lo tendrá)"; fail=1
    fi
done

# Cross-check the checksums too: abuild would fail on a stale sha512sums.
SUMS=$(mktemp "${TMPDIR:-/tmp}/nura-sums.XXXXXX")
sed -n '/^sha512sums="/,/^"/p' APKBUILD | sed '1d;$d' > "$SUMS"
if ! sha512sum -c "$SUMS" >/dev/null 2>&1; then
    echo "FAIL sha512sums no coinciden:" >&2
    sha512sum -c "$SUMS" 2>&1 | grep -v ': OK$' | head -10 >&2
    fail=1
fi
rm -f "$SUMS"

if [ "$fail" -ne 0 ]; then
    echo >&2
    echo "ERROR: el port no se puede construir en un runner limpio." >&2
    exit 1
fi
echo "port OK: ${#SRCS[@]} fuentes versionadas y con sha512 correctos"
