#!/usr/bin/env bash
# Build, package and upload a code-only iteration in one step.
#
#   bash ps4/deploy.sh
#   PS4_HOST=192.168.0.3 bash ps4/deploy.sh
#
# The package contains only the eboot (no assets): this build opts into
# PS4_EXTERNAL_DATA and reads /data/stk/data, uploaded once (see
# ps4/upload-data.py). Installing this package writes ~60 MB instead of the
# full game image. The public release is built with PS4_EXTERNAL_DATA=OFF.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
STK="$(cd "$HERE/.." && pwd)"
BUILD="${BUILD_DIR:-/tmp/stk-ps4-build}"
HOST="${PS4_HOST:-192.168.0.3}"
# The Package Installer keys on content ID: a package whose ID is already installed is refused
# even with a higher APP_VER, so version bumps do not buy a reinstall. The working loop is
# uninstall + install of this eboot-only package; see ps4-kit docs/ssd-friendly-iteration.md.
VERSION="${STK_VERSION:-00.01}"
cd "$STK"
echo "version: $VERSION"
PS4_EXTERNAL_DATA=ON BUILD_DIR="$BUILD" bash ps4/build.sh

PKG="$(BUILD_DIR="$BUILD" STK_VERSION="$VERSION" STK_PKG_EBOOT_ONLY=1 \
        bash ps4/pkg.sh | tail -1)"
[[ -f "$PKG" ]] || { echo "no package produced" >&2; exit 1; }

echo
echo "package: $PKG"
ls -l "$PKG"
sha256sum "$PKG"

curl -sS -T "$PKG" "ftp://$HOST:2121/data/pkg/$(basename "$PKG")" --user anonymous:anonymous
echo "uploaded to ftp://$HOST/data/pkg/$(basename "$PKG")"
echo
echo "Package is on the console. Uninstall the app, then install it with the Package Installer:"
echo "  /data/pkg/$(basename "$PKG")"
echo "The installer refuses a second install of the same content ID; replacing app.pkg over FTP fails."
