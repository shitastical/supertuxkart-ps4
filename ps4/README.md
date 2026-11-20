# SuperTuxKart on PS4

Port of [SuperTuxKart](https://github.com/supertuxkart/stk-code) to the PS4,
built with [iHaiDeeZ/ps4-kit](https://github.com/iHaiDeeZ/ps4-kit): clang 21 +
libc++ 21, Mesa's RADV, and the kit's VideoOut WSI.

The game is **minimally modified**: STK's own Vulkan renderer (`EDT_VULKAN`,
`GEVulkanDriver`) is used. The port supplies what STK expects from the
platform — SDL2 for window/events/input/audio, Vulkan surface creation, and a
filesystem root — while ps4-kit supplies the GPU stack, runtime and eboot.

## Status

| Piece | State |
|---|---|
| CMake integration (`PS4_PORT=ON`) | done, wired into the root build |
| Build/package scripts | done (`build.sh`, `pkg.sh`) |
| ps4-kit link + eboot | done (`ps4-post.cmake`) |
| SDL2 PS4 layer | implemented for STK's Vulkan WSI + input; cbfd SDL2 supplies window/events/audio |
| shaderc / freetype / harfbuzz / png / jpeg / curl / mbedTLS cross-builds | built and installed in the cbfd sysroot |
| Audio (SceAudioOut via MojoAL → SDL) | linked through SDL2; not separately verified in the playable run |
| Input (ScePad → SDL gamecontroller) | implemented (`ps4_input.cpp`); pad drives the menus and the tutorial |
| `.pkg` produced | yes — `out/IV0000-STKR00001_00-STKPORT000000000.pkg` (validated) |
| Runtime (2026-11-20) | **playable on hardware**: menus and tutorial load. Close path wired (`ps4_kit_exit`); verify on the next run. Failures, present contract and playbook: [`PORT-NOTES.md`](PORT-NOTES.md) |

`SDL_API_SURFACE.md` is the measured SDL2 surface STK needs: 188 symbols,
about 90 real functions (the rest are GL/atomic/thread stubs pulled in by
Irrlicht's SDL device). It is the implementation checklist for the layer.

## Prerequisites (on the build machine, normally WSL2)

1. ps4-kit, with its toolchain and Mesa built once:

   ```sh
   git clone https://github.com/iHaiDeeZ/ps4-kit ~/ps4-kit
   bash ~/ps4-kit/scripts/seed-build-tree.sh   # ~/ps4kit-build + sysroot
   bash ~/ps4-kit/gpu/build-mesa.sh            # static RADV
   ```

2. The port libraries in `$PS4_SYSROOT` — SDL2, shaderc, freetype, harfbuzz,
   libpng, libjpeg, zlib, curl, and OpenSSL (the seed sysroot has OpenSSL;
   mbedTLS is the fallback). The matrix and per-library notes are in
   `PORTING.md`.

3. Game data: `stk-code/` plus a checkout of `stk-assets`
   (`svn co https://svn.code.sf.net/p/supertuxkart/code/stk-assets`).

## Build

```sh
ps4/tools/preflight.sh      # says exactly what is still missing
ps4/build.sh                # configure + ninja
ps4/pkg.sh                  # full package: data + assets, into out/
STK_PKG_EBOOT_ONLY=1 ps4/pkg.sh  # iteration package; needs /data/stk/data
ps4/deploy.sh               # build + eboot-only package + upload to /data/pkg
```

`build.sh` sources `ps4-kit/toolchain/env.sh` and calls the kit's `ps4_cmake`,
so the compiler, sysroot and linker flags are the kit's, not the host's.
The build tree is `cmake_build-ps4/`.

Outputs:

- `cmake_build-ps4/supertuxkart` — ELF
- `cmake_build-ps4/supertuxkart_eboot/eboot.bin` — fake SELF for the package
- `out/IV0000-STKR00001_00-STKPORT000000000.pkg`

Title ID and content label in `pkg.sh` (`STKR00001` / `STKPORT`) are
placeholders; change them if you publish builds.

## How it fits together

```
STK main loop
 └─ Irrlicht CIrrDeviceSDL ── SDL2 (port layer) ── SceVideoOut / ScePad
 └─ GEVulkanDriver
     ├─ SDL_Vulkan_GetVkGetInstanceProcAddr ── ps4kit::vk_get_instance_proc_addr
     ├─ SDL_Vulkan_GetInstanceExtensions ───── VK_KHR_surface + VK_EXT_headless_surface
     ├─ SDL_Vulkan_CreateSurface ───────────── vkCreateHeadlessSurfaceEXT
     └─ SDL_Vulkan_GetDrawableSize ─────────── 1920x1080
 └─ MojoAL ── SDL audio ── SceAudioOut
```

A full package stages code data and assets into `/app0/data`; STK's compile-time
`SUPERTUXKART_DATADIR` default is `/app0`, and the PS4 launcher passes no
arguments. This is what `ps4/pkg.sh` builds by default, and it is the package
published in Releases. For iteration the port's constructor overrides that at
runtime with `SUPERTUXKART_DATADIR=/data/stk`, and `STK_PKG_EBOOT_ONLY=1`
packages only the executable, so the assets are uploaded once to
`/data/stk/data` instead of being rewritten on every install. See
`ps4-kit/docs/ssd-friendly-iteration.md`.

## Licensing

`stk-code` is GPLv3; ps4-kit is MIT. Files added under `ps4/` are part of the
GPLv3 port and can be distributed with the game.
