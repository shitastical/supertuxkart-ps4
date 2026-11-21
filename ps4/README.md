# SuperTuxKart on PS4

Port of [SuperTuxKart](https://github.com/supertuxkart/stk-code) to the PS4,
built with [iHaiDeeZ/ps4-kit](https://github.com/iHaiDeeZ/ps4-kit): clang 21 +
libc++ 21, Mesa's RADV, and the kit's VideoOut WSI.

The game is minimally modified: STK's own Vulkan renderer (`EDT_VULKAN`,
`GEVulkanDriver`) is used. The port supplies what STK expects from the
platform - SDL2 for window/events/input/audio, Vulkan surface creation, and a
filesystem root - while ps4-kit supplies the GPU stack, runtime and eboot.

The main project README has the full install and build instructions. This file
keeps the port-specific details.

## Status

| Piece | State |
|---|---|
| CMake integration (`PS4_PORT=ON`) | done, wired into the root build |
| Build/package scripts | done (`build.sh`, `pkg.sh`) |
| ps4-kit link + eboot | done (`ps4-post.cmake`) |
| SDL2 PS4 layer | implemented for STK's Vulkan WSI + input; cbfd SDL2 supplies window/events/audio |
| shaderc / freetype / harfbuzz / png / jpeg / curl / crypto | built and installed in the seed sysroot |
| Audio (SceAudioOut via MojoAL to SDL) | playable; 44.1 kHz mix, 1024-frame device buffer, deeper music queue |
| Input (ScePad to STK gamepads) | implemented (`ps4_input.cpp`); each pad is a separate device for menus, tutorial, races and split-screen |
| Text input | PS4 system IME via Cross on a focused field; current text is editable |
| Add-ons | drag-and-drop importer (`ps4_mods.cpp`); all three STK types are covered and local entries are registered; kart path hardware-tested |
| Runtime | playable on retail hardware: menus, tutorial, races and two-player split-screen verified |
| `.pkg` | full standalone package builds and installs |

`SDL_API_SURFACE.md` is the measured SDL2 surface STK needs: 188 symbols,
about 90 real functions (the rest are GL/atomic/thread stubs pulled in by
Irrlicht's SDL device).

## Build

The normal package is the self-contained one: executable plus all game data
and assets under `/app0/data`. It needs no separate data upload and uninstalling
the application removes the packaged files.

```sh
ps4/tools/preflight.sh
PS4_EXTERNAL_DATA=OFF ps4/build.sh
STK_VERSION=00.11 ps4/pkg.sh
```

Output: `out/IV0000-STKR00001_00-STKPORT000000000.pkg`.

`PS4_EXTERNAL_DATA=OFF` is required. `pkg.sh` checks the build cache and will
not package an external-data binary as a full release, and it refuses a full
package without a `stk-assets` checkout containing `tracks/`.

The development loop is separate:

```sh
PS4_EXTERNAL_DATA=ON BUILD_DIR="$HOME/stk-ps4-dev" ps4/build.sh
STK_PKG_EBOOT_ONLY=1 BUILD_DIR="$HOME/stk-ps4-dev" ps4/pkg.sh
```

That eboot-only package reads assets from `/data/stk/data` and is not a normal
install. Do not distribute it.

Build tree defaults to `cmake_build-ps4/`, or a space-free fallback under
`/tmp` when the checkout path contains spaces. Use `BUILD_DIR` to choose it
explicitly. `PS4_KIT`, `STK_ASSETS`, `STK_VERSION` and `JOBS` are documented in
the root README and the script headers.

## How it fits together

```text
STK main loop
 +- Irrlicht CIrrDeviceSDL -- SDL2 (port layer) -- SceVideoOut / ScePad
 +- GEVulkanDriver
     +- SDL_Vulkan_GetVkGetInstanceProcAddr -- ps4kit::vk_get_instance_proc_addr
     +- SDL_Vulkan_GetInstanceExtensions ----- VK_KHR_surface + VK_EXT_headless_surface
     +- SDL_Vulkan_CreateSurface ------------- vkCreateHeadlessSurfaceEXT
     +- SDL_Vulkan_GetDrawableSize ----------- 1920x1080
 +- MojoAL -- SDL audio -- SceAudioOut
```

`SUPERTUXKART_DATADIR` is pinned to `/app0` in the full build, so a release
binary cannot accidentally load a developer's `/data/stk` tree. The package
includes `sce_sys/about/right.sprx`; without it a fresh console can refuse to
launch the package.

Saves, profiles and the RADV shader cache remain in `/data/stk` so they survive
reinstalls. Delete `/data/stk` manually for a completely clean removal.

## Controls

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

The layout is a standard console racing layout: bottom face button accelerates, right face
button brakes, shoulders drift and fire. The left stick gives analog steering;
the D-pad gives full-lock digital steering. Each controller is registered as a
separate STK gamepad, so a second controller can join split-screen.

`KeyboardConfig::getBindingAsString` and `GamepadConfig::getBindingAsString`
have PS4 branches that return the actual DualShock 4 glyphs instead of key or
button names. The
glyphs come from `data/ttf/DS4Prompts.ttf`, a small icon font whose characters
live in the Private Use Area (U+E000-U+E009) and which is part of the
`normal-ttf` fallback chain in `stk_config.xml`. The glyphs are outlines, so
they tint with the surrounding text colour. Face buttons are the pad discs;
L1/L2/R1/R2 are keycaps with solid labels, large enough to read at prompt size.
Steering is one control, so both directions return the single left-stick disc
(U+E009).

Text fields use the PS4 system keyboard. It opens on Cross, starts with the
field's current text (password fields included) and replaces the field on
accept, so an existing value can be edited rather than retyped. Password fields
are drawn with a centred bullet mask on PS4 so the entered characters remain
visible.

## Add-ons (v0.11.0)

`ps4_mods.cpp` runs before the add-ons manager is created. It scans
`/data/stk/mods` and USB layouts, extracts zips or copies folders to
`/data/stk/supertuxkart/addons/{karts,tracks}`, and records sources in
`/data/stk/mods/.ps4-imported`. All three STK add-on types are covered: karts
use `kart.xml`; race tracks, battle arenas and soccer fields use `track.xml`.
Arenas and soccer fields live under `tracks/` because STK implements them as
tracks.

The Add-ons screen does not discover files by itself: it reads
`addons_installed.xml`. `AddonsManager::checkInstalledAddons()` now creates an
approved installed entry for a valid local kart, track or arena that is
missing from that file, then saves it. This is what makes drag-and-dropped
add-ons appear under **Installed mods**. Among Us was verified through the online browser;
a locally imported kart was verified and now races.

Local add-ons have no date field in their XML, so the date column uses the
add-on's filesystem modification time (normally the import date). Existing
`date="0"` entries are rewritten at startup. The date repair is implemented;
a separate post-fix on-screen check was not recorded.

## Known issues

- Split-screen was tested with two signed-in users after the pad fix. The
  port opens exactly one standard ScePad per signed-in user at index 0, which
  is how the PS4 assigns local players. The previous build also probed
  indices 1-3; that could return a handle that never reported a second
  physical pad and then occupied player 2's slot before the second user signed
  in. Sign the second controller in to a second PS4 user or guest, then press
  Cross on it at kart selection. If it still does not join, the login-user
  list and a build timestamp are written to `/data/ps4-kit/boot-trace.log`.
- Track, arena and soccer-field imports and the local-date display repair have
  not had separate hardware tests. The kart import and online add-on download
  paths were tested.
- Online multiplayer has not been tested yet.
- Do not change the controls or graphics settings. Those options have not been
  tested on PS4, and changing them may cause issues.

## Licensing

`stk-code` is GPLv3; ps4-kit is MIT. Files added under `ps4/` are part of the
GPLv3 port and can be distributed with the game.
