# Porting SuperTuxKart to the PS4

This is the engineering dossier for the port: what STK needs, what ps4-kit
provides, what has been wired, and what is still missing. It is written
against STK master (`b2a9f22`) and ps4-kit's documented API.

## 1. Why this shape works

STK already has a Vulkan renderer. `src/graphics/irr_driver.cpp` maps
`EDT_VULKAN` to `GEVulkanDriver` (`video::EDT_VULKAN`), and
`lib/graphics_engine/` is a complete Vulkan 1.2 backend: instance/device
creation, swapchain, pipelines, descriptor sets, and runtime GLSL → SPIR-V
compilation. Nothing in the renderer is OpenGL-specific.

ps4-kit supplies exactly the two things that backend lacks on a PS4:

- **A Vulkan driver.** RADV, built statically from the Mesa revision pinned
  by ps4-kit, with the kit's PS4 patches (VideoOut presentation, one in-order
  queue, no binary semaphores, swapchain slots 0–11).
- **A presentation path.** `VK_EXT_headless_surface` plus
  `VK_KHR_swapchain` on VideoOut, fixed 1920x1080, with flip pacing.

STK reaches Vulkan through SDL. The entire WSI dependency is four calls in
`lib/graphics_engine/src/ge_vulkan_driver.cpp`:

| Line | Call | PS4 implementation |
|---|---|---|
| ~106 | `SDL_Vulkan_GetVkGetInstanceProcAddr` | `ps4kit::vk_get_instance_proc_addr` |
| ~572 | `SDL_Vulkan_GetInstanceExtensions` | `VK_KHR_surface`, `VK_EXT_headless_surface` |
| ~745 | `SDL_Vulkan_CreateSurface` | `vkCreateHeadlessSurfaceEXT` |
| ~1248 | `SDL_Vulkan_GetDrawableSize` | 1920×1080 |

That is the seam. The rest of STK's SDL use comes from Irrlicht's SDL device
(`lib/irrlicht/source/Irrlicht/CIrrDeviceSDL.cpp`): window, displays, events,
keyboard/mouse, game controllers, timers, threads, and file I/O.

## 2. Verified facts

- Vulkan backend: `src/graphics/irr_driver.cpp` (`EDT_VULKAN`), preferred
  Vulkan 1.2, minimum 1.1.
- Shaders are compiled at runtime with shaderc
  (`ge_vulkan_shader_manager.cpp`). With `DISABLE_SHADERC`, `loadShader()`
  returns `VK_NULL_HANDLE`; Vulkan is unusable. **shaderc is mandatory.**
- Audio: `lib/mojoal/mojoal.c` calls `SDL_OpenAudioDevice`; there is no
  non-SDL backend in the tree. Sound therefore depends on the SDL layer.
- Asset lookup: `src/io/file_manager.cpp` checks directories beside the
  executable, then `SUPERTUXKART_DATADIR`, then `/usr/local/share/games`.
  Assets are discovered from `../stk-assets` relative to the data root or
  `SUPERTUXKART_ASSETS_DIR`; `--root` adds more roots.
- The renderer is chosen from the user config (`src/config/user_config.hpp`,
  default `"opengl"`). The port changes the PS4 default to `"vulkan"`.
- STK's CMake looks for SDL2 with `find_library(SDL2_LIBRARY)` /
  `find_path(SDL2_INCLUDEDIR)`; pre-seeding those cache entries is enough to
  point it at the sysroot.

## 3. What was added

| File | Purpose |
|---|---|
| `ps4/ps4-platform.cmake` | top-of-build integration: options, sysroot lookups, hard requirements (SDL2, shaderc, mbedTLS), datadir |
| `ps4/ps4-post.cmake` | end-of-build integration: `ps4_kit_link_mesa`, `ps4_kit_app`, datadir definition |
| `ps4/build.sh` | sources `toolchain/env.sh`, calls `ps4_cmake`, builds with ninja |
| `ps4/pkg.sh` | stages `eboot.bin` + data + assets, calls `scripts/make-pkg.sh` |
| `ps4/ps4_mods.cpp`, `ps4/ps4_mods.hpp` | drag-and-drop add-on importer; scans internal/USB, stages zips, installs karts/tracks |
| `ps4/tools/preflight.sh` | reports missing toolchain, sysroot libs, Mesa, assets, disk |
| `ps4/SDL_API_SURFACE.md` | 188-symbol SDL2 surface measured from STK |
| root `CMakeLists.txt` | `option(PS4_PORT)` + two guarded includes |
| `src/config/user_config.hpp` | PS4 default renderer = Vulkan |

The patch to STK itself is limited to the CMake hook, the two includes, the
renderer default, the add-on registration hook in `main.cpp`, and the
local-add-on registration in `src/addons/addon.*` and
`src/addons/addons_manager.cpp`.

## 4. Dependency matrix

| Capability | STK consumer | PS4 provider | State |
|---|---|---|---|
| Window, events, displays | Irrlicht SDL device | SceVideoOut + SceSystemService | SDL layer to write |
| Keyboard/mouse | Irrlicht SDL device | ScePad (pad mapping) | SDL layer to write |
| Game controller | `SDL_GameController*` | ScePad | SDL layer to write |
| Vulkan instance/device | GE Vulkan driver | ps4-kit RADV | provided by kit |
| Vulkan surface/swapchain | 4 SDL calls | ps4-kit WSI | seam wired |
| GLSL → SPIR-V | shaderc | cross-build | missing |
| Audio output | MojoAL | SceAudioOut via SDL audio | SDL layer to write |
| Fonts / text | freetype, harfbuzz | cross-build | missing |
| Images | libpng, libjpeg, zlib | cross-build | missing |
| Networking | curl, OpenSSL, enet | cross-build (enet bundled) | missing |
| Threads, timers, rwops | SDL2 | libc++ 21 / kernel | SDL layer to write |
| Save/config dir | FileManager | writable `/data` | open: HOME handling |
| Packaging | — | `scripts/make-pkg.sh` | wired |
| PhysFS | not used | — | — |

Missing libraries are ordinary cross-builds with the same toolchain:

```sh
cmake -DCMAKE_TOOLCHAIN_FILE=$PS4_KIT/toolchain/ps4-kit-toolchain.cmake \
      -DCMAKE_INSTALL_PREFIX=$PS4_SYSROOT -DBUILD_SHARED_LIBS=OFF ...
ninja install
```

zlib, libpng, libjpeg, freetype and harfbuzz are configure-and-install.
curl needs `--with-openssl` (the seed sysroot has it) and `--disable-shared`. shaderc must
be built with `-DSHADERC_SKIP_TESTS=ON -DSHADERC_SKIP_EXAMPLES=ON
-DBUILD_SHARED_LIBS=OFF`; install the static `shaderc_combined` and headers.

## 5. The SDL2 layer (implemented)

`SDL_API_SURFACE.md` lists the exact surface. It is not a full SDL2 port and
should not become one. Grouped by effort:

1. **Video + events** — SDL window object, 1920×1080 drawable, keyboard events,
   quit, focus, resize. Irrlicht only needs one window; no multiple displays,
   no GL context (Vulkan path).
2. **Vulkan** — the four `SDL_Vulkan_*` functions, backed by ps4-kit.
3. **Input** — `SDL_GameController`/joystick on ScePad, mapping the DualShock 4
   layout STK already understands; keyboard events can be synthesized from pad
   buttons for menus.
4. **Audio** — `SDL_OpenAudioDevice` + queue/callback on SceAudioOut, 48 kHz
   stereo. MojoAL does the mixing.
5. **Plumbing** — mutex/thread/timer/atomic stubs are mostly libc++ aliases;
   `SDL_RWops` over the kit's `*at` layer; `SDL_GetPrefPath`/`SDL_GetBasePath`
   pointing at `/app0` and `/data`.
6. **GL stubs** — Irrlicht's SDL device compiles GL code even when Vulkan is
   selected; the symbols only need to exist and return failure, never run.

> Status note (2026-11-20): this layer is implemented and tested on retail
> hardware. Vulkan surface creation, ScePad input, audio, the PS4 system IME and
> two-player split-screen all run. It is deliberately not stubbed with fake
> success returns: a fake `SDL_Vulkan_CreateSurface` would crash inside RADV
> with no useful report.

## 6. Milestones

| # | Goal | Pass condition |
|---|---|---|
| M0 | kit bring-up | `kit-test` runs on the console |
| M1 | STK configures + links | `supertuxkart_eboot/eboot.bin` exists |
| M2 | first frame | logo/menu visible, no GPU hang, crash log clean |
| M3 | playable menu + race | pad drives menus, a track loads, Vulkan presents at 60 Hz |
| M4 | audio + saves | music/SFX, config persists across boots |
| M5 | package | `.pkg` installs and launches from the XMB |

M1 cannot be reached without the missing libraries. M2 cannot be reached
without the SDL layer.

## 7. Open problems to solve during M2/M3

- **Save directory.** `checkAndCreateConfigDir()` needs a writable path. PS4
  homebrew has `/data`; verify whether it relies on `HOME` and, if so, redirect
  it to `/data/supertuxkart` early in `main()`.
- **Shader compile time.** shaderc at first boot on a Jaguar core is slow;
  STK's shader cache must land on writable storage (`/data`), otherwise every
  boot pays the cost.
- **Swapchain images.** The kit's WSI exposes slots 0–11 and no binary
  semaphores; STK's GE driver uses binary semaphores in its present path.
  Either RADV's emulation covers it or GE needs a small patch. Test at M2.
- **Pad mapping.** STK expects SDL controller buttons/axes; map ScePad's
  digital buttons and sticks, including the touch pad click.
- **Resolution.** Internal render can stay at 1280×720 while the WSI presents
  1920×1080; decide after first profiling.

## 8. Build and package

```sh
ps4/tools/preflight.sh
ps4/build.sh
STK_VERSION=00.11 ps4/pkg.sh
```

`build.sh` passes `-DPS4_PORT=ON`, `-DNO_SHADERC=OFF`, disables GLES2, Wii
support, the recorder, sqlite and system enet/squish/mcpp, and enables the
bundled MojoAL. `pkg.sh` merges `stk-code/data` with `stk-assets` into
`/app0/data`, adds the eboot, and calls the kit's `make-pkg.sh`.

## 9. Environment note

When this dossier was written, a `.pkg` had **not** been produced. On the
machine used then, ps4-kit was cloned at `~/ps4-kit`, but its build tree was
absent: no clang-21, no sysroot, no Mesa build, and
`/opt/pacbrew/ps4/openorbis` did not exist. The tree above is the integration
that runs on a machine where `seed-build-tree.sh` and `build-mesa.sh`
completed.

## 10. Build completed

> Historical note: this section records the first v0.01 package from
> 2026-11-19. The current v0.11.0 release is
> `out/SuperTuxKart-PS4-v0.11.0.pkg` (1,161,756,672 bytes,
> SHA-256 `953676340072d488bf1b99b6ad7d8c02ed1dc20844f1d1bf6c899a74905c827c`).
> It adds the add-on importer, the split-screen pad fix and local add-on date
> repair.

On 2026-11-19 the cbfd toolchain (`~/cbfd-build`) was used to build the port
and package it. The resulting file is
`out/IV0000-STKR00001_00-STKPORT000000000.pkg` (1,160,773,632 bytes).
`PkgTool.Core pkg_validate` passes all digest and signature checks; the
extracted PFS contains 5,594 files, and every `data/` file matches the staged
asset tree by SHA-256. The SFO reads `SuperTuxKart` / `STKR00001` / `00.01`.
The package includes all 44 tracks and 18 karts from the official 1.5 release.
No PS4 hardware was available, so the install/launch/runtime checks in M2-M5
remain unverified.

## 11. Add-on importer (v0.11.0)

`ps4/ps4_mods.cpp` runs before `new AddonsManager()` and scans
`/data/stk/mods` plus USB mounts for zips or unpacked karts/tracks. It stages
archives and publishes only a complete installation under
`/data/stk/supertuxkart/addons`; source paths and sizes go in
`/data/stk/mods/.ps4-imported`.

STK 1.5 has three installable add-on types: kart, track and arena. The
importer handles all three. Arenas and soccer fields are tracks internally,
so they use `track.xml` and are installed under `tracks/`; the loaded track
supplies the arena/soccer flags. There is no standalone library, music or cup
add-on type. A track's `<library>` scene assets are part of its folder and
are copied with it. Kart import is hardware-tested; track/arena import uses
the same path but has not had a separate hardware test.

The Add-ons screen reads `addons_installed.xml`, not the add-on directories.
`AddonsManager::checkInstalledAddons()` originally only marked entries that
were already in the online catalogue as installed, so a local `MarioKart`
folder was invisible. The port now adds an approved/installed local `Addon`
entry for valid karts, tracks and arenas missing from the catalogue, then
saves the file. Among Us (installed through the online browser) and Mario (local import)
were verified to load and race.

The first local registration left `date="0"` in `addons_installed.xml`, which
the screen rendered as 01/01/1970. Local kart/track XML has no date field, so
the manager uses the add-on's filesystem modification time (the importer's
install time) and repairs an existing zero-date installed entry at startup.
The date repair is implemented; a separate post-fix on-screen check was not
recorded.

## 12. Runtime bring-up (2026-11-20)

The port is **playable on retail hardware**: menus, tutorial, races and
two-player split-screen run. The v0.11.0 release has passed online Among Us
and local Mario install-and-race checks; the local date repair was not
separately re-checked on screen, and track/arena imports have no separate
hardware run. The close path is the kit's `ps4_kit_exit` →
`ps4kit_video_shutdown` sequence
(record a final close test on the next run). The playable result
depended on two runtime fixes that the build integration above does not show:

- the kit's present path called `sceGnmSubmitDone` before the VideoOut flip,
  which turned every frame into a 500 ms kernel deadline (1 fps) and eventually
  stopped submission completion during loading (system coredump, shell reboot);
- present waited the frame fence without a bound, so once the kernel stopped
  completing submissions the app froze behind a fence that could not signal.

The WSI now waits the fence at most 200 ms, submits the flip, and calls
`ac_ps4_flip_made(flipArg)` only when VideoOut reports that flip made. The kit
rejects a repeated or stale flip number, and the old `ac_ps4_submit_done()`
symbol is inert. The evidence, the port checklist, the log playbook and the
remaining known issues (fixed pipeline, heap arena cap) are in
[`PORT-NOTES.md`](PORT-NOTES.md).
