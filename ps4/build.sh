#!/usr/bin/env bash
# Build SuperTuxKart for PS4 with ps4-kit.
#
#   ps4/build.sh            # configure + build
#   ps4/build.sh clean      # wipe the build tree first
#
# Requires the kit on this machine:
#   git clone https://github.com/iHaiDeeZ/ps4-kit ~/ps4-kit
#   bash ~/ps4-kit/scripts/seed-build-tree.sh   # ~/ps4kit-build + sysroot
#   bash ~/ps4-kit/gpu/build-mesa.sh            # static RADV
# and the port libraries in $PS4_SYSROOT (see ps4/PORTING.md).
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
STK="$(cd "$HERE/.." && pwd)"
KIT="${PS4_KIT:-$HOME/ps4-kit}"

if [[ ! -f "$KIT/toolchain/env.sh" ]]; then
    echo "ps4-kit not found at $KIT" >&2
    echo "Set PS4_KIT or clone it to ~/ps4-kit (see ps4/README.md)." >&2
    exit 1
fi

# Reuse the local cbfd build's clang 21, libc++ sysroot and static RADV.
# The kit's env.sh keeps any values already exported here.  Override with
# PS4_SYSROOT / OPENORBIS / PS4_MESA_BUILD if your tree is elsewhere.
if [[ -f "$HOME/cbfd-build/env.sh" ]]; then
    # shellcheck source=/dev/null
    source "$HOME/cbfd-build/env.sh"
    export PS4_SYSROOT="${PS4_SYSROOT:-$HOME/cbfd-build/sysroot}"
    export OPENORBIS="${OPENORBIS:-$HOME/cbfd-build/openorbis}"
fi
# shellcheck source=/dev/null
source "$KIT/toolchain/env.sh"
# The kit's seed build is the one the kit patches (including the VideoOut flip
# accounting in ac_ps4_drm.c). cbfd's older Mesa archive can be missing
# ac_ps4_flip_made, which create-fself then rejects. Override explicitly if
# your kit build is elsewhere.
export PS4_MESA_BUILD="${PS4_MESA_BUILD:-${PSB:-$HOME/ps4kit-build}/build/mesa}"

for t in cmake ninja; do
    command -v "$t" >/dev/null || { echo "$t not found" >&2; exit 1; }
done
export PATH="$PSB/host-tools/bin:$PATH"

if [[ ! -f "$PS4_MESA_BUILD/src/amd/vulkan/libvulkan_radeon.a" ]]; then
    echo "Mesa/RADV is not built at $PS4_MESA_BUILD" >&2
    echo "Run: bash $KIT/gpu/build-mesa.sh" >&2
    exit 1
fi

# OpenOrbis adds an unquoted --script path to the link flags. A build tree
# under a directory with a space therefore produces a split ld.lld argument;
# use a space-free fallback for such checkouts (the source path itself is
# quoted correctly by CMake).
if [[ -n "${BUILD_DIR:-}" ]]; then
    BUILD="$BUILD_DIR"
elif [[ "$STK" == *" "* ]]; then
    BUILD="${TMPDIR:-/tmp}/stk-ps4-build"
else
    BUILD="$STK/cmake_build-ps4"
fi
[[ "${1:-}" == clean ]] && rm -rf "$BUILD"

# ps4_cmake() from env.sh: the kit's toolchain, Release, CFLAGS cleared.
ps4_cmake -S "$STK" -B "$BUILD" -G Ninja \
    -DPS4_PORT=ON \
    -DPS4_EXTERNAL_DATA="${PS4_EXTERNAL_DATA:-OFF}" \
    -DPS4_MESA_BUILD="$PS4_MESA_BUILD" \
    -DPS4_MESA_SRC="$PS4_SYSROOT" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCHECK_ASSETS=OFF \
    -DUSE_GLES2=OFF \
    -DBUILD_RECORDER=OFF \
    -DUSE_WIIUSE=OFF \
    -DUSE_SQLITE3=OFF \
    -DUSE_MOJOAL=ON \
    -DUSE_SYSTEM_ENET=OFF \
    -DUSE_SYSTEM_SQUISH=OFF \
    -DUSE_SYSTEM_MCPP=OFF \
    -DNO_SHADERC=OFF

ninja -C "$BUILD" -j"${JOBS:-$(nproc)}"

echo
echo "Built: $BUILD/supertuxkart"
echo "eboot: $BUILD/supertuxkart_eboot/eboot.bin"
echo "Package with: ps4/pkg.sh"
