# SuperTuxKart for PS4: v0.11.0

**Status: released.**

v0.11.0 is the standalone PS4 package release. It includes the add-on
importer, local add-on registration, the split-screen pad fix, and the local
add-on date repair.

## Changes from v0.10.0

v0.10.0 is withdrawn and is not a usable upgrade source. v0.11.0 replaces its
developer-data package with a standalone package and adds the add-on and
split-screen work.

### Standalone package and startup

- Switched the public build to `PS4_EXTERNAL_DATA=OFF`: the executable, STK
  code data, and the full STK 1.5 asset tree are under `/app0/data`.
- Included all 44 tracks and 18 karts from STK 1.5.
- Included `sce_sys/about/right.sprx`, which v0.10.0 omitted.
- Removed the `/data/stk/data`, FTP upload, and developer-eboot requirement.
  Install the `.pkg` and launch it from the home screen.
- Added packaging guards for the data mode, missing `tracks/`, and missing
  assets, plus a space-free build-directory fallback. The content-ID and
  friendly package names are hard links to the same file.
- Kept saves, profiles, settings, installed add-ons, and the shader cache in
  `/data/stk`.

### Add-ons

- Added the startup importer for internal storage and USB.
- Supports `.zip` archives and unpacked folders, add-on collections, and nested
  add-on ZIPs.
- Handles karts, race tracks, battle/CTF arenas, and soccer fields. Arenas and
  soccer fields are tracks internally and install under `tracks/`.
- Supports root-XML ZIPs by using the archive base name as the install folder.
- Stages and publishes without exposing a partial install, refuses symbolic
  links, and restores the previous install if the final rename fails.
- Records each source path and a size/modification-time stamp in
  `/data/stk/mods/.ps4-imported`, so unchanged sources are skipped.
- Registers locally imported karts, tracks, and arenas in
  `addons_installed.xml` so they appear on the Add-ons screen.
- Repairs missing or zero local add-on dates from the installed folder's
  modification time instead of showing 01/01/1970.

### Split-screen and input

- Opens one ScePad per signed-in user and exposes each pad as a real STK
  gamepad. This fixes the phantom player-2 slot that blocked the second pad.
- Tested two-player split-screen with two signed-in users and two DualShock 4
  controllers.
- Added DS4 prompt glyphs and the console racing-game controller layout.
- The input layer has four pad slots; three- and four-player split-screen were
  not tested.
- Suppresses the first-frame Cross press on the race screen and releases a pad
  when its user signs out.

### Text, IME, and UI

- The PS4 system keyboard opens from text fields, starts with the field's
  current text, and replaces it on accept.
- Password fields use a bullet mask instead of exposing the entered text.
- Updated the input settings, press-a-key dialog, and PS4 text handling in the
  registration and lobby screens. Online multiplayer remains untested.

### Audio and runtime

- Reworked music playback to address slowdown and crackling. Audio was
  verified only on the tested output path.
- Uses the RADV shader and pipeline cache under `/data/stk/cache` instead of
  recompiling shaders on every launch.
- Added PS4 tracing and watchdog logging, pthread/semaphore glue, and disabled
  Discord rich presence.

### Documentation

- Replaced the public README with the detailed tested matrix, install and
  add-on paths, build instructions, data layout, and caveats.
- Added these v0.11.0 release notes and updated the port documentation.

## Download

```text
File:    SuperTuxKart-PS4-v0.11.0.pkg
Size:    1,161,756,672 bytes
SHA-256: 953676340072d488bf1b99b6ad7d8c02ed1dc20844f1d1bf6c899a74905c827c
```

The package is attached to the
[v0.11.0 GitHub release](https://github.com/shitastical/supertuxkart-ps4/releases/tag/v0.11.0).

The package is self-contained: the executable and all game data are under
`/app0/data`. It includes the 44 tracks and 18 karts from SuperTuxKart 1.5. No
FTP upload or development data tree is needed.

## Highlights

- **Full standalone package.** Install the `.pkg` and launch it from the home
  screen. Saves and add-ons stay in `/data/stk`.
- **Split-screen pad fix.** One standard `ScePad` is opened per signed-in user,
  so a second controller can join instead of occupying a phantom player-2
  slot. Two-player split-screen was tested.
- **Drag-and-drop add-ons.** Karts, race tracks, battle arenas and soccer
  fields can be imported from internal storage or USB. Whole `.zip` files and
  unpacked folders work; no unzipping or renaming is required.
- **Local add-on registration.** Add-ons copied outside the online catalogue
  are registered in `addons_installed.xml` and appear under **Installed mods**.
- **Local add-on dates.** Installed-folder modification time is used instead of
  `date="0"`, which used to display as 01/01/1970. Existing zero-date entries
  are repaired at startup.
- **PS4 input and text fixes.** DS4 prompt glyphs, the PS4 system keyboard and
  editable text fields are wired into the port.

## Verified on hardware

- Full package install and launch on the test console (PS4 slim, firmware
  13.52).
- Main menu and tutorial.
- Local races and packaged tracks.
- Two-player split-screen with two signed-in users and two controllers.
- Analog steering, face buttons and shoulder buttons.
- Audio output, including changing the audio settings.
- In-game **Add-ons** catalogue: Among Us downloaded, installed, loaded and
  raced.
- Manual add-on import: `example_kart.zip` imported, appeared under
  **Installed mods**, and raced.
- Saves, profiles and settings persisted in `/data/stk` across reinstalling
  the application.

## Implemented but not independently verified

- **Track, arena and soccer-field imports.** They use the same `track.xml`
  import and registration path as karts, and registration runs after the track
  manager loads, but only the kart path was exercised end to end.
- **Local add-on date display.** The fix is present in this package, but the
  exact on-screen date was not separately re-recorded after the change.
- **PS4 system keyboard / IME flow.** Implemented, including editing existing
  text, but not included in the final release test matrix.
- **Final close/shutdown sequence.** Launch, play and reinstall were tested;
  a dedicated final close-and-exit test was not recorded.

## Install

1. Verify the SHA-256 above.
2. Uninstall the old `STKR00001` application. Keep `/data/stk`.
3. Install the `.pkg` with the PS4 package installer.
4. Launch SuperTuxKart from the home screen.

The PS4 package installer refuses an already-installed content ID. Do not use
`ps4/deploy.sh` or an `STK_PKG_EBOOT_ONLY=1` build for a normal install; those
are development packages that expect external assets.

Uninstalling removes `/app0`. It does not remove `/data/stk`; delete that
directory manually for a clean removal of saves, settings and add-ons.

## Add-ons

There are two supported paths.

### In-game curated catalogue

Open **Add-ons**, choose **Karts**, **Tracks** or **Arenas**, and download from
the SuperTuxKart catalogue. The game checks the add-on against its catalogue
and installs it into `/data/stk/supertuxkart/addons`. Among Us was tested this
way. This requires internet access and SuperTuxKart's online services.

### Manual drag and drop

Copy a `.zip` or unpacked add-on folder to one of:

```text
/data/stk/mods/
/mnt/usb0/mods/
/mnt/usb0/SuperTuxKart/mods/
/mnt/usb0/STK/mods/
/mnt/usb0/PS4/mods/
```

`Mods`, `addons`, and the corresponding `SuperTuxKart/...`, `STK/...` and
`PS4/...` variants are also scanned. USB mounts `usb0` through `usb7` are
accepted. Restart the game after copying files.

| Type | Required file | Installed under |
|---|---|---|
| Kart | `kart.xml` | `addons/karts/` |
| Race track | `track.xml` | `addons/tracks/` |
| Battle/CTF arena | `track.xml` with `arena="Y"` | `addons/tracks/` |
| Soccer field | `track.xml` with `soccer="Y"` | `addons/tracks/` |

Arenas and soccer fields are tracks internally, so they live under `tracks/`.
There is no standalone library add-on type in SuperTuxKart 1.5; `<library>`
entries are part of a track scene and are copied with the track. Standalone
library, music and cup packs are not add-ons.

Limits:

- `.zip` only; `.7z` and `.rar` are not supported.
- `kart.xml` and `track.xml` must be lowercase.
- Nested folders are scanned up to four levels deep.
- Multiple add-ons in one zip or folder are supported, including nested add-on
  zips.
- A later add-on with the same destination name replaces the earlier one.
- If an installed add-on is deleted while its source remains, delete
  `/data/stk/mods/.ps4-imported` (or the source) to force a re-import.

## Caveats

- Controls settings and graphics settings have not been tested. Do not change
  them; the release is only verified with the default mapping and default
  graphics.
- Online multiplayer is untested.
- Only a PS4 slim on firmware 13.52 was tested.
- The game and every track/kart combination were not exhaustively tested.
- Track/arena/soccer-field import has no separate hardware test yet.
- The local date display fix was not independently re-checked on screen.
- Add-on content can have its own missing assets. The tested kart mod logs
  missing `genericshadow.png` and `kart_exhaust.xml` references; it still loads
  and races.
- The final close/shutdown path does not yet have a dedicated repeat test.

## Build

The release uses the self-contained build:

```sh
ps4/tools/preflight.sh
PS4_EXTERNAL_DATA=OFF ps4/build.sh
STK_VERSION=00.11 ps4/pkg.sh
```

See the [main README](README.md) for prerequisites, asset checkout setup and
package installation details.

## License

SuperTuxKart is GPLv3. This is an unofficial port and is not affiliated with
the SuperTuxKart project.
