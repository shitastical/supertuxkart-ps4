# SuperTuxKart for PS4

An unofficial port of [SuperTuxKart](https://supertuxkart.net/) to the
PlayStation 4, built with [ps4-kit](https://github.com/iHaiDeeZ/ps4-kit),
Mesa's RADV driver, and the console's VideoOut display path.

The game uses SuperTuxKart's own Vulkan renderer. This repository adds the PS4
platform layer (SDL2 video, input and audio, Vulkan surface creation, the
filesystem root), the build and packaging scripts, and the runtime notes.

## Status

Playable on a retail PS4. Menus, races and the game's maps work. This is an
unofficial homebrew port, not an official SuperTuxKart release, and it has not
been thoroughly tested for bugs. The current test console is a PS4 slim on
13.52 firmware.

There is **no public package release at the moment**. The older v0.10.0 package
was withdrawn because it was built for the development data layout and did not
contain the assets. Do not install it. Build the current full package from
source with the instructions below.

## Build a normal, completely installable package

The normal build is a self-contained `.pkg`: the executable and all game data
and assets are staged under `/app0/data` inside the package. Installing it
needs no FTP upload, no `/data/stk/data` tree, and no development package. When
the console uninstalls the application, the packaged files under `/app0` are
removed with it.

### Prerequisites

Build on Linux (WSL2 is fine). You need:

- [ps4-kit](https://github.com/iHaiDeeZ/ps4-kit) with its clang 21 toolchain,
  OpenOrbis runtime, sysroot and Mesa/RADV built once.
- The port libraries in the ps4-kit sysroot: SDL2, shaderc, freetype,
  harfbuzz, libpng, libjpeg, zlib, curl and OpenSSL (mbedTLS also works).
  The seed sysroot supplies these; [`ps4/PORTING.md`](ps4/PORTING.md) has the
  dependency matrix if a library is missing.
- SuperTuxKart's [asset checkout](https://github.com/supertuxkart/stk-assets)
  with `tracks/`. Put it next to the source directory or set `STK_ASSETS`.
- About 8 GiB of free disk space and a space-free build directory. The scripts
  use `cmake_build-ps4/` by default and automatically fall back to a temporary
  space-free path when the checkout path contains spaces.

Build ps4-kit once:

```sh
git clone https://github.com/iHaiDeeZ/ps4-kit "$HOME/ps4-kit"
bash "$HOME/ps4-kit/scripts/seed-build-tree.sh"
bash "$HOME/ps4-kit/gpu/build-mesa.sh"
```

Build the game and the full package:

```sh
git clone https://github.com/shitastical/supertuxkart-ps4.git
cd supertuxkart-ps4

# The default is ../stk-assets. Use STK_ASSETS for another checkout.
git clone https://github.com/supertuxkart/stk-assets.git ../stk-assets

ps4/tools/preflight.sh          # prints anything still missing, installs nothing
PS4_EXTERNAL_DATA=OFF ps4/build.sh
STK_VERSION=00.11 ps4/pkg.sh
```

The package is written to:

```text
out/IV0000-STKR00001_00-STKPORT000000000.pkg
```

`PS4_EXTERNAL_DATA=OFF` is the default and is required for a normal package.
`ps4/pkg.sh` checks the build cache and refuses to package a development
binary as a full release. It also refuses to build a "full" package without a
`stk-assets` checkout containing `tracks/`.

Useful overrides:

| Variable | Meaning |
|---|---|
| `PS4_KIT` | ps4-kit checkout; default `~/ps4-kit` |
| `BUILD_DIR` | build tree; default `cmake_build-ps4` or a space-free fallback |
| `STK_ASSETS` | asset checkout; default `../stk-assets` |
| `STK_VERSION` | package content version, in the form `XX.YY` |
| `JOBS` | parallel build jobs; default the host CPU count |

For example:

```sh
BUILD_DIR="$HOME/stk-ps4-build" STK_VERSION=00.11 ps4/build.sh
BUILD_DIR="$HOME/stk-ps4-build" STK_VERSION=00.11 ps4/pkg.sh
```

### Install and uninstall

Copy the `.pkg` to the PS4, install it with your package installer, and launch
SuperTuxKart from the home screen. The full package contains the game data, so
there is nothing else to upload.

Uninstalling the application removes the executable and the packaged
`/app0/data` assets. SuperTuxKart stores user data separately under
`/data/stk/` (configuration, profiles and the RADV shader cache). The PS4
package format has no uninstall hook for that writable directory, so delete
`/data/stk/` manually if you want a completely clean removal, including saves
and settings.

Do **not** use `ps4/deploy.sh` or `STK_PKG_EBOOT_ONLY=1` for a normal install.
Those produce the small development iteration package, which expects assets at
`/data/stk/data` and is not standalone.

## Controls

The current layout is Mario Kart 8 Deluxe's:

| Input | Menus | Race |
|---|---|---|
| Left stick / D-pad | Navigate | Steer |
| Cross | Select | Accelerate |
| Circle | Back | Brake / reverse |
| R2 | - | Fire item |
| L2 | - | Drift |
| L1 | - | Nitro |
| Triangle | - | Rescue |
| Square | - | Look back |
| Options | Back | Pause |

The left stick steers with analog precision; the D-pad remains a full-lock
digital alternative. Each connected PS4 controller appears as its own gamepad,
so split-screen can assign one controller per player. Cross on a focused text
field opens the PS4 system keyboard. The keyboard starts with the field's
current text, including password fields, and the result replaces the old
contents so any field can be edited. The face-button and shoulder prompts use
the DualShock 4 glyphs in `data/ttf/DS4Prompts.ttf`.

## Known issues

- Split-screen controller detection is implemented but has not been confirmed
  on retail hardware yet. If a second player cannot join, please report it.
- Online multiplayer has not been tested yet.
- Do not change the controls or graphics settings. Those options have not been
  tested on PS4, and changing them may cause issues.

## Technical notes

Build and runtime details are in [`ps4/PORTING.md`](ps4/PORTING.md) and
[`ps4/PORT-NOTES.md`](ps4/PORT-NOTES.md). The port source is under `ps4/`.
The upstream SuperTuxKart README is kept as
[`README.stk.md`](README.stk.md).

## Credits and license

SuperTuxKart is by the SuperTuxKart team and contributors. ps4-kit is by
iHaiDeeZ. The port uses Mesa/RADV, SDL2, OpenOrbis and the cbfd toolchain.

The code is distributed under the GNU General Public License v3; see
[`COPYING`](COPYING). Game data is under a mixture of licenses, also described
in `COPYING`. This is an unofficial port and is not affiliated with the
SuperTuxKart project.
