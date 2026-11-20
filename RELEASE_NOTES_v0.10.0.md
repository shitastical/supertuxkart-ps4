# SuperTuxKart for PS4 — v0.10.0

An unofficial homebrew port of [SuperTuxKart](https://supertuxkart.net/) to the
PlayStation 4, built with [ps4-kit](https://github.com/iHaiDeeZ/ps4-kit),
Mesa's RADV driver and STK's Vulkan renderer.

Playable on a real PS4: races, menus, and all of the maps seem to work fine.
This is not an official SuperTuxKart release, and it has not been thoroughly
tested for bugs. Expect rough edges. Tested on a PS4 slim on 13.52 firmware.

## Install

A PS4 with homebrew package installation enabled is required.

Download `SuperTuxKart-PS4-v0.10.0.pkg` below (1.1 GiB). This is the **full**
package: the game data and assets are included under `/app0/data`, so no
separate `/data/stk` upload is needed for a normal install. Install it with
your package installer and launch SuperTuxKart from the home screen.

Saves and configuration are kept under `/data/stk/config-0.10/`.

## Known issues

- **Music sometimes slows down and the audio crackles.** Audio playback is not
  fully stable yet. Until this is fixed, turn the music volume down to 0 in the
  game's audio options.
- **Controls are not very good.** The controller bindings aren't the best right
  now and will be improved later.

## Package details

- File: `SuperTuxKart-PS4-v0.10.0.pkg`
- Size: `1,160,839,168` bytes (1.1 GiB)
- SHA-256: `7f4354f2afe57e0d71ed8b0a3a8ce4371986f1a321b6855d490a313ed1b90182`
- Title ID: `STKR00001`
- Content label: `STKPORT`
- Package version: `00.10`
- The package was validated with OpenOrbis `PkgTool.Core`; all package,
  content, header and signature checks passed.

## Notes

This is a homebrew package, not an official SuperTuxKart release. It was built
from the hardware-playable source tree documented in the repository's README
and `ps4/PORT-NOTES.md`. No fresh PS4 install was performed for this release
because no console is attached to this build machine; the prior playable
hardware run and this package validation are the available evidence.

Build and runtime details are in the repository:
<https://github.com/shitastical/supertuxkart-ps4>
