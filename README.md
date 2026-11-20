# SuperTuxKart for PS4

A port of [SuperTuxKart](https://supertuxkart.net/) to the PlayStation 4,
built on [ps4-kit](https://github.com/iHaiDeeZ/ps4-kit) with Mesa's RADV
driver and the console's VideoOut display path.

SuperTuxKart's own Vulkan renderer is used. This repository adds the PS4
platform layer (SDL2 video/input/audio, Vulkan surface creation, filesystem
root), the build and packaging scripts, and the runtime notes.

## Status

Playable on a real PS4: Races, menus, and all of the maps seem to work fine. This is an
unofficial homebrew port, not an official SuperTuxKart release, and I have not thoroughly tested it for bugs. Expect rough
edges. Tested on a PS4 slim on 13.52 firmware.

## Install

A PS4 with homebrew package installation enabled is required.

Prebuilt packages will be attached to Releases after the port is in a more stable state. Until then, build one with the instructions below, install it with
your package installer, and launch SuperTuxKart from the home screen.

Saves and configuration are kept under `/data/stk/config-0.10/`.

## Controls

The DualShock 4 is mapped to SuperTuxKart's keyboard bindings:

| Input | Action |
|---|---|
| D-pad / left stick | Menus, steering, accelerate (up) and brake (down) |
| Cross | Select / confirm |
| Circle or Options | Back / pause |
| Square | Fire |
| L1 or R1 | Drift |

Cross on a focused text field opens the system keyboard.

## Known issues

- **Music sometimes slows down and the audio crackles.** Audio playback is not fully
  stable yet. Until this is fixed, it is recommended to turn the music off in
  the game's audio options (set the music volume to 0).

- **Controls are not very good.** The controller bindings aren't the best right now.
  This will be fixed later.

## Build from source

Requirements:

- ps4-kit with its toolchain, sysroot and Mesa/RADV built
  (`scripts/seed-build-tree.sh`, then `gpu/build-mesa.sh`)
- the port libraries listed in [`ps4/PORTING.md`](ps4/PORTING.md)
- SuperTuxKart's asset checkout (`stk-assets`) next to this directory

```sh
git clone https://github.com/shitastical/supertuxkart-ps4.git
cd supertuxkart-ps4
# place stk-assets at ../stk-assets
ps4/build.sh
ps4/pkg.sh
```

`ps4/build.sh` writes to `cmake_build-ps4/` (or a space-free fallback under
`/tmp` when the checkout path contains spaces). `ps4/pkg.sh` writes the
package to `out/`.

Runtime notes, the present-path contract and the debugging playbook are in
[`ps4/PORT-NOTES.md`](ps4/PORT-NOTES.md); the integration dossier is
[`ps4/PORTING.md`](ps4/PORTING.md). The upstream project README is kept as
[`README.stk.md`](README.stk.md).

## Credits and license

SuperTuxKart is by the SuperTuxKart team and contributors. ps4-kit is by
iHaiDeez. The port uses Mesa/RADV, SDL2, OpenOrbis and the cbfd toolchain.

The code is distributed under the GNU General Public License v3; see
[`COPYING`](COPYING). Game data is under a mixture of licenses, also described
in `COPYING`. This is an unofficial port and is not affiliated with the
SuperTuxKart project.
