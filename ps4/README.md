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
| Input (ScePad to SDL gamecontroller) | implemented (`ps4_input.cpp`); pad drives the menus, tutorial and races |
| Text input | PS4 system IME via Cross on a focused field; current text is editable |
| Runtime | playable on retail hardware: menus, tutorial and races load |
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

The layout is Mario Kart 8 Deluxe's: bottom face button accelerates, right face
button brakes, shoulders drift and fire. Steering is digital; there is no
analog steering curve yet.

`KeyboardConfig::getBindingAsString` has a PS4 branch that returns the actual
DualShock 4 glyphs instead of the key names the pad is translated into. The
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

## Known issues

- Online multiplayer and split-screen have not been tested yet.
- Do not change the controls or graphics settings. Those options have not been
  tested on PS4, and changing them may cause issues.

## Licensing

`stk-code` is GPLv3; ps4-kit is MIT. Files added under `ps4/` are part of the
GPLv3 port and can be distributed with the game.
