#!/usr/bin/env bash
# Inventory of every SDL2 symbol STK (incl. bundled Irrlicht, GE and MojoAL) references.
# Usage: ps4/tools/sdl_surface.sh > ps4/SDL_API_SURFACE.md
set -euo pipefail
cd "$(dirname "$0")/../.."
echo "# SDL2 API surface required by STK"
echo
echo "Generated from the checked-out source by \`ps4/tools/sdl_surface.sh\`."
echo "A PS4 SDL2 layer must define every symbol listed here."
echo
for area in "src" "lib/irrlicht/source" "lib/graphics_engine/src" "lib/mojoal"; do
  echo "## $area"
  echo
  echo "| symbol | refs |"
  echo "|---|---:|"
  grep -rhoE "SDL_[A-Za-z0-9_]+[[:space:]]*\(" "$area" 2>/dev/null \
    | sed 's/[[:space:]]*($//' | sort | uniq -c | sort -rn \
    | awk '{printf "| `%s` | %s |\n", $2, $1}'
  echo
done
