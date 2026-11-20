#!/usr/bin/env bash
# Preflight for the SuperTuxKart PS4 port.
# Prints what is present and what is missing. Exit status: 0 if a build can
# be attempted, 1 otherwise. Installs nothing.
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
STK="$(cd "$HERE/../.." && pwd)"
KIT="${PS4_KIT:-$HOME/ps4-kit}"
SYSROOT="${PS4_SYSROOT:-$HOME/ps4kit-build/sysroot}"
MESA="${PS4_MESA_BUILD:-$HOME/ps4kit-build/build/mesa}"
missing=0

ok()   { printf "  [ok]      %s\n" "$*"; }
bad()  { printf "  [missing] %s\n" "$*"; missing=1; }
warn() { printf "  [warn]    %s\n" "$*"; }

echo "ps4-kit"
[[ -f "$KIT/toolchain/env.sh" ]] && ok "$KIT" || bad "$KIT (clone iHaiDeeZ/ps4-kit)"
command -v clang-21 >/dev/null && ok "clang-21" || bad "clang-21 (seed-build-tree.sh)"
command -v ld.lld-21 >/dev/null && ok "ld.lld-21" || bad "ld.lld-21 (seed-build-tree.sh)"
[[ -f "${OPENORBIS:-/opt/pacbrew/ps4/openorbis}/lib/crt1.o" ]] \
    && ok "OpenOrbis runtime" || bad "OpenOrbis runtime (${OPENORBIS:-/opt/pacbrew/ps4/openorbis})"

echo "sysroot: $SYSROOT"
[[ -d "$SYSROOT" ]] && ok "sysroot" || bad "sysroot (seed-build-tree.sh)"
for lib in libSDL2.a libshaderc_combined.a libz.a \
           libfreetype.a libharfbuzz.a libpng.a libjpeg.a libcurl.a; do
    [[ -e "$SYSROOT/lib/$lib" ]] && ok "$lib" || bad "$SYSROOT/lib/$lib"
done
if [[ -e "$SYSROOT/lib/libssl.a" && -e "$SYSROOT/lib/libcrypto.a" ]]; then
    ok "OpenSSL"
elif [[ -e "$SYSROOT/lib/libmbedcrypto.a" ]]; then
    ok "mbedTLS"
else
    bad "crypto: OpenSSL (libssl.a/libcrypto.a) or libmbedcrypto.a"
fi
[[ -e "$SYSROOT/include/SDL2/SDL.h" ]] && ok "SDL2 headers" || bad "$SYSROOT/include/SDL2"
[[ -e "$SYSROOT/include/shaderc/shaderc.hpp" ]] && ok "shaderc headers" || bad "$SYSROOT/include/shaderc"

echo "Mesa / RADV"
[[ -f "$MESA/src/amd/vulkan/libvulkan_radeon.a" ]] \
    && ok "libvulkan_radeon.a" || bad "$MESA (gpu/build-mesa.sh)"

echo "game data"
[[ -d "$STK/data" ]] && ok "$STK/data" || bad "$STK/data"
for d in gui skins shaders; do
    [[ -d "$STK/data/$d" ]] && ok "data/$d" || bad "$STK/data/$d (sparse checkout?)"
done
if [[ -d "${STK_ASSETS:-$STK/../stk-assets}/tracks" ]]; then
    ok "stk-assets"
else
    warn "no stk-assets (build works; game has no tracks)"
fi

echo "disk"
avail_kb=$(df -Pk "$STK" | awk 'NR==2 {print $4}')
avail_gib=$(awk -v k="$avail_kb" 'BEGIN { printf "%.1f", k/1048576 }')
if (( avail_kb > 8 * 1024 * 1024 )); then
    ok "$avail_gib GiB free"
else
    bad "$avail_gib GiB free; STK's build tree needs ~8 GiB"
fi

echo
if (( missing )); then
    echo "Not ready to build. See ps4/PORTING.md for the dependency matrix."
    exit 1
fi
echo "Ready: ps4/build.sh"
