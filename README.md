# SuperTuxKart for PS4

An unofficial port of [SuperTuxKart](https://supertuxkart.net/) to the
PlayStation 4. It uses SuperTuxKart's Vulkan renderer, Mesa's RADV driver, the
console's VideoOut display path, and [ps4-kit](https://github.com/iHaiDeeZ/ps4-kit).

This repository contains the PS4 platform layer, the build and packaging
scripts, the drag-and-drop add-on importer, and the runtime notes. It is not an
official SuperTuxKart release.

## v0.11.0 release

This is the current release. It is a complete, standalone package: the
executable and all game data are inside the package under `/app0/data`. It does
not need an FTP data upload or a development build.

```text
File:    SuperTuxKart-PS4-v0.11.0.pkg
Size:    1,161,756,672 bytes
SHA-256: 953676340072d488bf1b99b6ad7d8c02ed1dc20844f1d1bf6c899a74905c827c
```

Download it from the
[v0.11.0 GitHub release](https://github.com/shitastical/supertuxkart-ps4/releases/tag/v0.11.0).
The release also provides a checksum file.

The package includes the official SuperTuxKart 1.5 assets: all 44 tracks and
18 karts. The tested console is a PS4 slim on firmware 13.52.

## Verified working

These items were actually exercised on the test console:

- Installing and launching the full standalone package.
- The main menu and tutorial.
- Local races, including the packaged tracks.
- Two-player split-screen with two signed-in users and two controllers.
- DualShock 4 input: analog steering, face buttons and shoulder buttons.
- Audio output.
- Downloading and installing an add-on through the in-game **Add-ons**
  browser. Among Us was installed this way, then loaded and raced.
- Manually importing a kart without unzipping it. `mariokart_06315.zip` was
  imported, appeared under **Installed mods**, and was raced.
- Saves, profiles and settings persisting in `/data/stk` when the application
  is reinstalled.

The add-on implementation also covers race tracks, battle/CTF arenas and
soccer fields. Those follow the same import and registration flow as karts,
but only the kart path has been exercised end to end. The local add-on date
repair is implemented but was not independently re-checked after the fix. See
[Caveats and untested areas](#caveats-and-untested-areas).

## Install the game

### 1. Verify the download

On Linux or macOS:

```sh
sha256sum SuperTuxKart-PS4-v0.11.0.pkg
```

On Windows:

```powershell
certutil -hashfile SuperTuxKart-PS4-v0.11.0.pkg SHA256
```

The result must be:

```text
953676340072d488bf1b99b6ad7d8c02ed1dc20844f1d1bf6c899a74905c827c
```

### 2. Remove the old application

Uninstall the existing `STKR00001` installation first. The PS4 package
installer refuses an already-installed content ID.

Keep `/data/stk` when reinstalling. It contains saves, profiles,
configuration, the RADV shader cache, imported add-ons and the add-on state
file. The package does not contain your user data.

### 3. Install and launch

Copy the `.pkg` to the PS4 and install it with your package installer. Launch
SuperTuxKart from the home screen. No files need to be uploaded to `/data` for
the game itself.

The package is built with `PS4_EXTERNAL_DATA=OFF`. Do not use
`ps4/deploy.sh`, `STK_PKG_EBOOT_ONLY=1`, or a development-data build for a
normal install; those expect game assets at `/data/stk/data` and are only for
iteration.

### Uninstall

Uninstalling the application removes the executable and the packaged files
under `/app0`. It does not remove `/data/stk`. Delete `/data/stk` manually if
you want a completely clean removal, including saves, settings and add-ons.

## Add-ons

There are two supported ways to install add-ons:

1. Use the in-game **Add-ons** menu to download from SuperTuxKart's curated
   online catalogue.
2. Copy a `.zip` or unpacked add-on folder to a mods directory and let the
   port's importer install it at the next launch.

Both paths put the add-on under the writable add-ons directory:

```text
/data/stk/supertuxkart/addons/karts/<name>/
/data/stk/supertuxkart/addons/tracks/<name>/
```

### Option 1: download from the in-game Add-ons menu

This is the simplest path when the console has internet access and the add-on
is in SuperTuxKart's approved catalogue. Among Us was tested this way.

1. Start SuperTuxKart and open **Add-ons**.
2. Choose the add-on category: **Karts**, **Tracks** or **Arenas**.
3. Select an add-on and choose to download/install it.
4. Wait for the download and installation to finish.
5. Open **Installed mods** to confirm it is listed. A downloaded kart or track
   can then be selected in the normal game menus.

The catalogue is operated by the SuperTuxKart project. It is curated and
version-checked by the game, so it is preferable when you want a known
compatible add-on and do not want to manage files yourself. It requires a
working network connection and SuperTuxKart's online services.

### Option 2: manually drag and drop a mod

No unzipping or renaming is required. The importer scans these locations at
startup:

```text
/data/stk/mods/                         internal storage
/mnt/usb0/mods/                         USB drive root
/mnt/usb0/SuperTuxKart/mods/            common USB layout
/mnt/usb0/STK/mods/                     alternate USB layout
/mnt/usb0/PS4/mods/                     alternate USB layout
```

`Mods`, `addons`, `SuperTuxKart/addons`, `STK/addons` and the matching
`PS4/...` variants are accepted too. USB drives mounted as `usb0` through
`usb7` are scanned.

Copy a `.zip` or an already-unpacked folder into one of those directories with
FTP, a file manager, or the USB drive itself. Restart SuperTuxKart after adding
files; the scan happens at boot. The source file or folder is not moved or
deleted.

| You provide | What the importer does |
|---|---|
| `mariokart_06315.zip` | Checks the archive, extracts it to staging, then installs the kart under `addons/karts/`. This is the tested example. |
| `MarioKart/` containing `kart.xml` | Copies the folder as an installed kart. |
| A track or arena file containing `track.xml` | Installs it under `addons/tracks/`. |
| A folder containing `karts/<id>/kart.xml` and/or `tracks/<id>/track.xml` | Installs each valid add-on it finds. |
| A folder containing several mod zips | Imports each zip. |
| A zip containing other add-on zips | Extracts the outer zip and imports each inner add-on. |

The importer remembers each source path and its size/timestamp in
`/data/stk/mods/.ps4-imported`. An unchanged source is not copied again on
every boot. Replace the source to import a new version. If you delete an
installed add-on but leave its source in place, delete the state file (or the
source) if you want it to import again.

A later add-on with the same destination name replaces the earlier one. The
in-game display name comes from the add-on's XML; the installed folder name is
derived from the add-on folder or archive name.

### Supported add-on types

SuperTuxKart 1.5 has three installable add-on types, and the port handles all
of them:

| Type | Required file | Installed under |
|---|---|---|
| Kart | `kart.xml` | `karts/` |
| Race track | `track.xml` | `tracks/` |
| Battle/CTF arena | `track.xml` with `arena="Y"` | `tracks/` |
| Soccer field | `track.xml` with `soccer="Y"` | `tracks/` |

Arenas and soccer fields are technically tracks in SuperTuxKart. That is why
they live under `tracks/`; the loaded track supplies the arena or soccer flag.

There is no separate library add-on type in SuperTuxKart 1.5. `<library>`
entries are part of a track's scene and are copied with that track. Standalone
library, music and cup packs are not SuperTuxKart add-ons and are ignored.

### Manual add-on limits

- `.zip` only. `.7z`, `.rar` and other archive formats are not supported.
- `kart.xml` and `track.xml` must use those lowercase names.
- Nested folders are scanned up to four levels deep.
- A folder that already contains a valid add-on is treated as that add-on.
  Extra zips inside that same folder are ignored; put a mod pack in its own
  folder or zip.
- A failed, corrupt, unsafe or partially extracted archive is rejected instead
  of publishing a half-installed add-on.
- The archive format must be a normal single-volume ZIP. Password-protected
  archives are not supported.
- The add-on must be packaged for SuperTuxKart 1.5. An old or incompatible mod
  can load incorrectly or fail to appear even when its files were copied.
- Manual installs do not have online catalogue metadata such as designer,
  description or icon. The name comes from the add-on XML and the date comes
  from the installed folder's modification time, normally the import date.

If a manually copied add-on does not appear, confirm that it contains the
correct lowercase XML file, that it is within the scan depth, and that the
game was restarted after copying it. Removing it from the installed add-ons
folder and restarting is safe; the source remains untouched.

### Local add-on dates

Local mods have no date field in `kart.xml` or `track.xml`. The port uses the
installed folder's filesystem modification time, normally the import date, and
repairs an existing `date="0"` entry at startup instead of showing
01/01/1970.

## Controls

The default layout is based on Mario Kart 8 Deluxe:

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

The left stick steers with analog precision; the D-pad is a full-lock digital
alternative. Each connected PS4 controller appears as its own gamepad, so
split-screen can assign one controller per player.

For split-screen, sign the second controller in to a second PS4 user or guest,
then press Cross on it at kart selection. If a second pad does not join, the
login-user list and a build timestamp are written to
`/data/ps4-kit/boot-trace.log`.

Cross on a focused text field opens the PS4 system keyboard. It starts with the
field's current text, including password fields, and the result replaces the old
contents.

## Caveats and untested areas

These are the boundaries of the release verification, not a claim that
everything else is broken:

- **Controls settings and graphics settings are untested.** Do not change them.
  The port has only been verified with the default control mapping and default
  graphics settings. Changing renderer, resolution, effects or input settings
  may cause crashes, a black screen or unplayable input.
- **Online multiplayer is untested.** Local races and split-screen were tested;
  network play, accounts and online matchmaking were not.
- **Track, arena and soccer-field imports have not had a separate hardware
  test.** They use the `track.xml` importer and the same local registration
  flow as karts, and registration runs after the track manager loads, but only
  the kart path and the online add-on path have been exercised end to end.
- **The local add-on date repair has not been independently re-checked.** The
  new package uses the installed folder's modification time and repairs
  existing zero dates, but the exact on-screen date after this change was not
  separately recorded.
- **The PS4 system keyboard/IME flow is implemented but not in the final test
  matrix.** It opens from a focused text field and supports editing existing
  text; it was not separately re-verified for this release.
- **The final shutdown path has not had a dedicated repeat test.** Launch,
  gameplay and reinstall/data persistence were tested. A final close-and-exit
  check should be recorded on the next test run.
- **Only one console/firmware combination was tested:** a PS4 slim on 13.52.
  Other models or firmware versions are unverified.
- **The game is not exhaustively tested.** The main menu, tutorial, local races
  and two-player split-screen were exercised. Other modes and every track/kart
  combination were not individually checked.
- **Add-on compatibility is the mod author's responsibility.** Packaged assets
  from SuperTuxKart 1.5 are included, but old or broken add-ons can still fail.
  For example, the tested Mario mod logs missing `genericshadow.png` and
  `kart_exhaust.xml` references; it still loads and races, but those are mod
  content warnings.
- **Uninstalling the game does not delete saves or add-ons.** `/data/stk` must
  be removed manually for a clean wipe.

## Build from source

The release package is self-contained: the executable and all game data and
assets are staged under `/app0/data`. The source build uses
`PS4_EXTERNAL_DATA=OFF` and the asset checkout.

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
  use `cmake_build-ps4/` by default and fall back to a temporary space-free
  path when the checkout path contains spaces.

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

ps4/tools/preflight.sh          # reports missing dependencies, installs nothing
PS4_EXTERNAL_DATA=OFF ps4/build.sh
STK_VERSION=00.11 ps4/pkg.sh
```

The package is written to:

```text
out/IV0000-STKR00001_00-STKPORT000000000.pkg
```

`ps4/pkg.sh` refuses to package an external-data binary as a full release and
refuses a full package without a `stk-assets` checkout containing `tracks/`.

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

## Where data lives

| Path | Contents |
|---|---|
| `/app0/data` | Packaged game assets; removed when the application is uninstalled |
| `/data/stk` | Saves, profiles and configuration |
| `/data/stk/mods` | Manual add-on drop directory and `.ps4-imported` state |
| `/data/stk/supertuxkart/addons` | Installed karts and tracks |
| `/data/stk/cache` | RADV shader/pipeline cache |

## Technical notes

The port uses SuperTuxKart's own Vulkan renderer (`EDT_VULKAN`,
`GEVulkanDriver`). ps4-kit supplies the GPU stack, VideoOut WSI, runtime and
eboot. The port supplies SDL2 video, input and audio, Vulkan surface creation,
the filesystem root, the add-on importer, and the writable data layout.

The add-on importer is `ps4/ps4_mods.cpp`; the local registration and date
handling are in `src/addons/addon.*` and `src/addons/addons_manager.cpp`.
Build and runtime details are in [`ps4/PORTING.md`](ps4/PORTING.md) and
[`ps4/PORT-NOTES.md`](ps4/PORT-NOTES.md). The upstream SuperTuxKart README is
kept as [`README.stk.md`](README.stk.md). Release notes are in
[`RELEASE_NOTES_v0.11.0.md`](RELEASE_NOTES_v0.11.0.md).

## Credits and license

SuperTuxKart is by the SuperTuxKart team and contributors. ps4-kit is by
iHaiDeeZ. The port uses Mesa/RADV, SDL2, OpenOrbis and the cbfd toolchain.

The code is distributed under the GNU General Public License v3; see
[`COPYING`](COPYING). Game data is under a mixture of licenses, also described
in `COPYING`. This is an unofficial port and is not affiliated with the
SuperTuxKart project.
