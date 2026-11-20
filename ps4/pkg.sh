#!/usr/bin/env bash
# Stage the PS4 build and package it as out/SuperTuxKart-vXX.YY.pkg.
#
#   STK_VERSION=00.01 ps4/pkg.sh
#
# Needs a built cmake_build-ps4/ (ps4/build.sh) and the game assets:
#   ../stk-assets  (svn co https://svn.code.sf.net/p/supertuxkart/code/stk-assets)
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
STK="$(cd "$HERE/.." && pwd)"
KIT="${PS4_KIT:-$HOME/ps4-kit}"
[[ -f "$KIT/scripts/make-pkg.sh" ]] || {
    echo "ps4-kit not found at $KIT (set PS4_KIT)" >&2; exit 1; }

# Reuse the same cbfd toolchain as build.sh. Its OpenOrbis tree has the
# packaging tools; the rebuild of create-gp4 there preserves spaces in asset
# names and implements -path (the bundled binary's help is misleading).
if [[ -f "$HOME/cbfd-build/env.sh" ]]; then
    # shellcheck source=/dev/null
    source "$HOME/cbfd-build/env.sh"
    export PS4_SYSROOT="${PS4_SYSROOT:-$HOME/cbfd-build/sysroot}"
    export OPENORBIS="${OPENORBIS:-$HOME/cbfd-build/openorbis}"
    export PSB="${PSB:-$HOME/cbfd-build}"
fi
# shellcheck source=/dev/null
source "$KIT/toolchain/env.sh"
if [[ -z "${CREATE_GP4:-}" && -x "$PSB/tools/create-gp4" ]]; then
    export CREATE_GP4="$PSB/tools/create-gp4"
fi

VERSION="${STK_VERSION:-00.00}"
if [[ -n "${BUILD_DIR:-}" ]]; then
    BUILD="$BUILD_DIR"
elif [[ "$STK" == *" "* ]]; then
    BUILD="${TMPDIR:-/tmp}/stk-ps4-build"
else
    BUILD="$STK/cmake_build-ps4"
fi
STAGE="$BUILD/stage"
EBOOT="$BUILD/supertuxkart_eboot/eboot.bin"

[[ -f "$EBOOT" ]] || { echo "missing $EBOOT (run ps4/build.sh)" >&2; exit 1; }
[[ -d "$STK/data" ]] || { echo "missing $STK/data" >&2; exit 1; }

rm -rf "$STAGE"
mkdir -p "$STAGE/sce_sys"

cp "$EBOOT" "$STAGE/eboot.bin"

# Normally the assets ride inside the package (installed at /app0/data).
# STK_PKG_EBOOT_ONLY=1 leaves them out: the port then reads /data/stk/data, which
# is uploaded once over FTP, so a code iteration installs only the eboot.
if [[ "${STK_PKG_EBOOT_ONLY:-0}" != "1" ]]; then
    # STK's FileManager reads /app0/data (SUPERTUXKART_DATADIR="/app0").
    # Code data and the asset tree share that directory in release tarballs.
    cp -a "$STK/data" "$STAGE/data"

    # Assets are a separate checkout (tracks/, karts/, textures/, ...). When it is
    # present, merge it into data/ so the game finds everything from one root.
    ASSETS="${STK_ASSETS:-$STK/../stk-assets}"
    if [[ -d "$ASSETS/tracks" ]]; then
        cp -a "$ASSETS/." "$STAGE/data/"
    else
        echo "warning: no stk-assets at $ASSETS; the game will start but have" >&2
        echo "         no tracks/karts. Set STK_ASSETS to the checkout." >&2
    fi
else
    echo "eboot-only package: assets are read from /data/stk/data" >&2
fi

# Icon is optional for make-pkg, but without it the XMB tile is blank.
# Use the port's own icon, else STK's 512x512 logo from the data tree.
ICON="$HERE/sce_sys/icon0.png"
[[ -f "$ICON" ]] || ICON="$STK/data/supertuxkart_512.png"
[[ -f "$ICON" ]] || { echo "no icon0.png and no data/supertuxkart_512.png" >&2; exit 1; }
cp "$ICON" "$STAGE/sce_sys/icon0.png"

# make-pkg.sh requires TITLE_ID = 4 letters + 5 digits.
TITLE_ID="STKR00001"
LABEL="STKPORT"
# STK_OUT keeps a large build off a full root disk (the package is ~1.2 GB).
OUT="${STK_OUT:-$STK/out}"
mkdir -p "$OUT"
PKG="$(NO_RIGHT_SPRX=1 bash "$KIT/scripts/make-pkg.sh" \
        "$STAGE" "$TITLE_ID" "SuperTuxKart" "$VERSION" "$LABEL" \
        "$OUT" | tail -1)"

echo "$PKG"
