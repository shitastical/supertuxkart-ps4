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

# The data root is a build-time choice. A release package must not contain a
# binary that reads /data/stk, and an eboot-only package must not contain one
# that looks for data inside /app0 that was deliberately left out.
EXTERNAL_DATA=""
if [[ -f "$BUILD/CMakeCache.txt" ]]; then
    EXTERNAL_DATA="$(sed -n 's/^PS4_EXTERNAL_DATA:BOOL=//p' "$BUILD/CMakeCache.txt" | tail -1)"
fi
if [[ "$EXTERNAL_DATA" != "ON" && "$EXTERNAL_DATA" != "OFF" ]]; then
    echo "cannot tell whether $BUILD uses /data/stk (PS4_EXTERNAL_DATA is '$EXTERNAL_DATA')" >&2
    echo "re-run ps4/build.sh with PS4_EXTERNAL_DATA=OFF for a release or ON for eboot-only" >&2
    exit 1
fi
if [[ "${STK_PKG_EBOOT_ONLY:-0}" == "1" && "$EXTERNAL_DATA" != "ON" ]]; then
    echo "eboot-only package needs PS4_EXTERNAL_DATA=ON; $BUILD was built for packaged data" >&2
    exit 1
fi
if [[ "${STK_PKG_EBOOT_ONLY:-0}" != "1" && "$EXTERNAL_DATA" != "OFF" ]]; then
    echo "full package needs PS4_EXTERNAL_DATA=OFF; $BUILD was built to read /data/stk" >&2
    exit 1
fi

rm -rf "$STAGE"
mkdir -p "$STAGE/sce_sys"

cp "$EBOOT" "$STAGE/eboot.bin"

# The release package carries the assets in /app0/data, so uninstalling the
# app removes them. STK_PKG_EBOOT_ONLY=1 is the development loop: it leaves the
# assets out and the PS4_EXTERNAL_DATA build reads /data/stk/data, uploaded once
# over FTP, so a code iteration installs only the eboot.
if [[ "${STK_PKG_EBOOT_ONLY:-0}" != "1" ]]; then
    # A "full" package with no tracks is not a release; fail instead of
    # silently shipping an app that has nothing to load.
    ASSETS="${STK_ASSETS:-$STK/../stk-assets}"
    if [[ ! -d "$ASSETS/tracks" ]]; then
        echo "missing assets at $ASSETS (need the stk-assets checkout with tracks/)" >&2
        echo "set STK_ASSETS to that checkout; refusing to build an incomplete release" >&2
        exit 1
    fi

    # STK's FileManager reads /app0/data (SUPERTUXKART_DATADIR="/app0").
    # The asset tree supplies tracks/karts/music/etc.; the code's data/ is
    # the overlay tied to this source revision and must be copied last. A
    # stale stk-assets checkout can otherwise replace options_video.stkgui
    # with an older copy that lacks benchmarkSelect, crashing the Video tab
    # in release builds where asserts are compiled out.
    mkdir -p "$STAGE/data"
    cp -a "$ASSETS/." "$STAGE/data/"
    cp -a "$STK/data/." "$STAGE/data/"

    if ! grep -q 'id="benchmarkSelect"' \
        "$STAGE/data/gui/screens/options/options_video.stkgui"; then
        echo "staged options_video.stkgui is missing benchmarkSelect;" >&2
        echo "the code data overlay did not win over the asset tree" >&2
        exit 1
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
mkdir -p "$STK/out"
# make-pkg.sh adds sce_sys/about/right.sprx unless NO_RIGHT_SPRX=1. Keep it:
# a fresh console can refuse to launch a package that omits the entitlement.
PKG="$(bash "$KIT/scripts/make-pkg.sh" \
        "$STAGE" "$TITLE_ID" "SuperTuxKart" "$VERSION" "$LABEL" \
        "$STK/out" | tail -1)"

echo "$PKG"
