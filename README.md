# SuperTuxKart for PS4

An unofficial port of [SuperTuxKart](https://supertuxkart.net/) to the PlayStation 4. It ships as a single standalone package that contains the game and all of its data. Nothing has to be uploaded to the console separately.

**Upstream version: SuperTuxKart 1.5.** The port is built from the SuperTuxKart source tree at commit `b2a9f22` (3 November 2026) and bundles the official 1.5 asset set: 44 tracks and 18 karts. Add-on compatibility is documented against 1.5.

**Tested on one console: a PS4 slim running firmware 13.52.** No other model or firmware has been tested. Read [Tested configuration](#tested-configuration) and [Caveats](#caveats) before installing.

This project is not affiliated with, endorsed by, or approved by Sony Interactive Entertainment or the SuperTuxKart team.

---

## Contents

1. [Release](#release)
2. [What works](#what-works)
3. [Tested configuration](#tested-configuration)
4. [Install](#install)
5. [Controls](#controls)
6. [Add-ons](#add-ons)
7. [Caveats](#caveats)
8. [Troubleshooting](#troubleshooting)
9. [Where data lives](#where-data-lives)
10. [Build from source](#build-from-source)
11. [Technical notes](#technical-notes)
12. [Credits and license](#credits-and-license)
13. [More documentation](#more-documentation)

---

## Release

Current release: **v0.11.0**
([release page](https://github.com/shitastical/supertuxkart-ps4/releases/tag/v0.11.0))

| Item | Value |
|---|---|
| Public version | `v0.11.0` |
| Package build version | `00.11` |
| Package file | `SuperTuxKart-PS4-v0.11.0.pkg` |
| Checksum file | `SuperTuxKart-PS4-v0.11.0.pkg.sha256` |
| Size | 1,161,756,672 bytes (about 1.08 GiB) |
| SHA-256 | `953676340072d488bf1b99b6ad7d8c02ed1dc20844f1d1bf6c899a74905c827c` |
| Title ID | `STKR00001` |
| Content ID | `IV0000-STKR00001_00-STKPORT000000000` |
| Display title | `SuperTuxKart` |
| Target | PS4 |
| Contents | Executable, game data under `/app0/data`, package metadata |
| Upstream base | SuperTuxKart `stk-code` at commit `b2a9f223326422aa2dd4eea28319b0c5115fde03` |
| Release commit | `6f02f58d4fcda4521dc2bd50073f778e3054d855` |

The package is large because it contains the complete game assets. A size of about 1.16 GB is expected and is not a sign of a corrupt download.

**v0.10.0 is withdrawn.** It used a developer data layout (game data expected outside the package) and omitted `sce_sys/about/right.sprx`, so it was not a usable public release. There is no migration step between v0.10.0 and v0.11.0. Uninstall v0.10.0 and install v0.11.0.

---

## What works

The following was verified on hardware with the v0.11.0 package on the test console described below.

| Area | Verified result |
|---|---|
| Installation | The standalone package installs through the PS4 package installer. |
| Launch | The installed game starts from the home screen. |
| Menus | The main menu renders and can be navigated with a DualShock 4. |
| Tutorial | The tutorial loads and is playable. |
| Local races | Local races load and run. |
| Split-screen | Two-player split-screen works with two signed-in users and two DualShock 4 controllers. Both players can race. |
| Driving input | Analog steering and acceleration, face and shoulder buttons, sticks, and menu navigation work on the tested paths. |
| Audio | Audio output works on the tested paths. Earlier music slowdown and crackling were addressed in the port. |
| Audio settings | Changing audio settings works; the new values take effect. |
| Curated add-ons | The in-game Add-ons menu reached the catalogue, downloaded an add-on, installed it, and launched it. Tested with an Among Us kart, including racing with it. |
| Manual add-ons | Manual import of a third-party kart archive (`example_kart.zip`) worked: it was imported at startup, appeared in the game, and raced. |
| Saves | Profiles, settings, and saves persist across launches in `/data/stk`. |
| Data layout | No separate asset upload and no developer data directory are needed. |

This list is deliberately narrow. It does not mean every mode, track, kart, or add-on works. No track compatibility matrix exists.

### Performance

On the tested PS4 slim at the release's default settings, the in-game benchmark reports **31 FPS steady** and **42 FPS typical**. These are the only measured numbers. A PS4 Pro was not tested; 60 FPS there is a reasonable expectation but is not verified.

---

## Tested configuration

| Item | Tested |
|---|---|
| Console | PS4 slim |
| Firmware | 13.52 |
| Video output | 1920x1080 |
| Controllers | DualShock 4 |
| Users | Two local users signed in for split-screen |
| Install method | PS4 package installer |
| Data location | `/data/stk` |

**Not tested:**

- PS4 original, PS4 Pro, PS5, PS5 Pro
- Any firmware other than 13.52
- Devkit or testkit consoles
- Installing to an external HDD
- Remote Play
- Suspend/resume and rest mode
- HDR, 4K, other resolutions, other refresh rates
- 3 or 4 player split-screen
- Keyboard, mouse, wheels, flight sticks, third-party controllers
- Online and LAN multiplayer
- Long-session stability

If your console or firmware differs from the tested one, treat the port as untested on your setup.

---

## Install

This is an unofficial, homebrew package. It is not licensed or approved by Sony, and no formal audit of the package signature or contents has been done. Your console must already be set up to install homebrew packages. This README does not cover how to do that.

### 1. Download

From the [release page](https://github.com/shitastical/supertuxkart-ps4/releases/tag/v0.11.0), download both:

- `SuperTuxKart-PS4-v0.11.0.pkg`
- `SuperTuxKart-PS4-v0.11.0.pkg.sha256`

### 2. Verify the download

Expected SHA-256:

```text
953676340072d488bf1b99b6ad7d8c02ed1dc20844f1d1bf6c899a74905c827c
```

Linux, with both files in the same directory:

```bash
sha256sum -c SuperTuxKart-PS4-v0.11.0.pkg.sha256
```

Expected output:

```text
SuperTuxKart-PS4-v0.11.0.pkg: OK
```

macOS:

```bash
shasum -a 256 -c SuperTuxKart-PS4-v0.11.0.pkg.sha256
```

It prints the same `OK` line. Or compute the hash directly:

```bash
sha256sum SuperTuxKart-PS4-v0.11.0.pkg
# or, on macOS
shasum -a 256 SuperTuxKart-PS4-v0.11.0.pkg
```

Windows PowerShell:

```powershell
Get-FileHash .\SuperTuxKart-PS4-v0.11.0.pkg -Algorithm SHA256
```

The file size should be exactly 1,161,756,672 bytes. If the hash or size differs, download the file again and do not install it.

A matching hash shows that your download is the published file. It does not show Sony certification.

### 3. Remove older builds

If an older PS4 SuperTuxKart app is installed (including the withdrawn v0.10.0), uninstall it first. The v0.10.0 package is not a usable public install.

Uninstalling the app does not necessarily delete `/data/stk`, so your saves and add-ons can stay in place. See [Uninstall and reset](#uninstall-and-reset).

### 4. Check free space

Make sure the console has room for a package of about 1.16 GB, plus space for add-ons and the shader cache.

### 5. Install

Copy the `.pkg` to the console in whatever way you normally use for homebrew packages, then install it with the PS4 package installer. The package is standalone. You do not need to copy game data to the console, run an FTP server for the game to start, or use a developer eboot.

### 6. Launch

Start **SuperTuxKart** from the home screen.

The first launch after installing may take longer. The port may compile Vulkan shaders and store them in a cache at `/data/stk/cache`. Later launches reuse the cache. This explains the delay; it is not a benchmark and no timing was measured.

### Update

Uninstall the previous app if it is installed, then install the new package. Keep `/data/stk` to keep saves and add-ons. Save compatibility across earlier builds was not tested; only persistence across launches was verified.

### Uninstall and reset

- **Uninstall the app:** removes the package. It does not necessarily remove `/data/stk`.
- **Full reset:** remove `/data/stk` separately. This deletes profiles, settings, saves, installed add-ons, imported mods state, and the shader cache. Do not do this unless you want all of that gone.

---

## Controls

The port uses a console racing-game layout. These are the defaults on the tested driving and menu paths:

| Action | DualShock 4 |
|---|---|
| Accelerate | Cross |
| Brake / reverse | Circle |
| Steer left / right | Left stick, left / right; D-pad also steers |
| Drift | L2 |
| Use item | R2 |
| Nitro | L1 |
| Look back | Square |
| Rescue | Triangle |
| Pause | Options |
| Menu navigation | D-pad or left stick |
| Menu confirm | Cross |
| Menu back | Circle or Options |

**Changing controls settings is untested.** SuperTuxKart contains a controls settings screen and may expose more actions than the table lists. That screen was not tested on the PS4 port. Remapping, applying changes, and whether changes persist are all untested. The screen may open, and that alone is not proof that changing settings is safe. Leave controls at the defaults.

Not supported or not tested: touchpad, gyro and motion, lightbar, PS button actions, keyboard, mouse, wheels, and third-party controllers.

### Split-screen

Two-player split-screen was tested with two DualShock 4 controllers and two signed-in local users.

1. Sign in both local users at the PS4 user screen before launching the game.
2. Connect both DualShock 4 controllers.
3. Start the game and set up a local split-screen race.
4. Player 1 uses the first assigned pad, player 2 the second.

Each player needs their own signed-in local user. A second controller without a second signed-in user is not a tested configuration. If the second pad is not recognized, sign in both users on the PS4 user screen and relaunch the game.

The input layer has four pad slots, but only two-player split-screen was tested. Three and four players are not claimed to work.

---

## Add-ons

SuperTuxKart add-ons can be installed two ways on this port:

1. **In-game Add-ons menu.** Downloads from the official curated catalogue. Tested.
2. **Manual copy of a mod.** Copy a ZIP or unpacked folder to a scan location, then launch the game. Tested with one kart ZIP.

"Drag and drop" in this README means copying a ZIP or folder to the console with a file manager, FTP, or USB. It does not mean dropping a file onto a running game.

Both paths target SuperTuxKart 1.5-era content. Add-ons built for very different game versions can be imported and still be incompatible.

### Supported add-on types

| Type | Identifying file | Installed to |
|---|---|---|
| Kart | `kart.xml` | `/data/stk/supertuxkart/addons/karts/<name>/` |
| Track | `track.xml` | `/data/stk/supertuxkart/addons/tracks/<name>/` |
| Battle or capture-the-flag arena | `track.xml` with `arena="Y"` | `/data/stk/supertuxkart/addons/tracks/<name>/` |
| Soccer field | `track.xml` with `soccer="Y"` | `/data/stk/supertuxkart/addons/tracks/<name>/` |

Notes on the table:

- Arenas and soccer fields install under `tracks/`. There is no separate arena directory. The game tells them apart by the `arena` and `soccer` attributes in `track.xml`.
- SuperTuxKart 1.5 has no standalone library, music, or cup add-on type. A `<library>` is scene content inside a track.
- XML file names are lowercase and case-sensitive: `kart.xml` and `track.xml`. Do not use `Kart.xml` or `TRACK.XML`.

Only the kart path was tested on hardware. Track, arena, and soccer import are implemented but not independently verified. See [Implemented but not verified](#implemented-but-not-verified).

### Path 1: the in-game Add-ons menu

Use this for known-compatible content.

1. Connect the PS4 to the internet.
2. Launch SuperTuxKart.
3. Open **Add-ons**.
4. Browse or search the catalogue.
5. Select an add-on, download it, and install it.
6. Use it in the matching mode (karts in kart selection, tracks and arenas in the track selection for the appropriate mode).

The downloaded files are stored under `/data/stk/supertuxkart/addons`. Do not unpack curated downloads by hand.

What was tested: browsing the catalogue, downloading and installing an Among Us kart, and racing with it.

Limits:

- It needs a network connection and a reachable official add-on service. Availability of that service is outside this project's control and can change.
- The catalogue is not every mod on the internet.
- Most catalogue items were not individually tested.
- Respect each add-on's license.

### Path 2: manual copy

Use this for a mod that is not in the catalogue.

1. Get a correctly packaged SuperTuxKart 1.5 add-on as a `.zip` archive or an unpacked folder.
2. Check that `kart.xml` or `track.xml` is present in a supported layout (see below).
3. Copy the `.zip` or the folder into a scan location. The simplest choice is `/data/stk/mods` on the console. For USB, create a `mods` folder at the root of the drive.
4. If you use USB, connect and mount the drive before launching the game.
5. Launch SuperTuxKart. The importer runs at startup. Large mods take longer to import.
6. Open the Add-ons screen or the kart or track selection and look for the new content.
7. If it is missing, check the layout rules below, then restart the game.

The importer scans at startup, so copy the files first and then launch or restart the game. Copying while the game is running does not make the mod appear until the next launch.

What was tested: `example_kart.zip`, a third-party kart archive, copied into the manual import area. It imported at startup, appeared in the game, and raced.

#### Accepted layouts

An archive or folder can contain:

- one add-on with `kart.xml` or `track.xml` at its root;
- one add-on inside a single top-level folder;
- a collection under `karts/` or `tracks/`.

The importer looks up to 4 directory levels deep. It skips dot-prefixed entries, `__MACOSX`, and `installed`/`Installed`.

A ZIP with `kart.xml` or `track.xml` directly at its root has no folder name to use, so the importer falls back to the archive's base name as the install directory name. Of these ZIPs, only the tested sample kart archive is confirmed on hardware.

#### What the importer does

1. Extracts a ZIP to a temporary staging area, or uses the source folder directly.
2. Checks that at least one `kart.xml` or `track.xml` root exists.
3. Copies the add-on to a temporary `.ps4-new` directory next to its destination.
4. Refuses symbolic links. It does not follow links out of the mod.
5. Publishes the add-on by renaming any existing destination to `.ps4-old`, renaming the new copy into place, and deleting the old one. If the final rename fails, it tries to restore the old copy.
6. Records the source path and a stamp derived from its size and modification time in `/data/stk/mods/.ps4-imported`.
7. Leaves your source archive or folder where it was.

Consequences:

- An unchanged source is skipped on later launches.
- A changed source is imported again and replaces the installed add-on with the same name.
- There is one installed directory per add-on name. Two mods with the same name overwrite each other.
- A failed import is meant to leave the last good version in place. If it does not, please report it.

The same-name replace and rollback path exists in code but was not independently exercised on hardware.

#### Manual import limits

- ZIP only. `.7z`, `.rar`, and `.tar` archives are not read.
- The importer does not download missing dependencies or missing assets, and it does not repair broken XML. A mod with missing files can fail or look wrong.
- Not guaranteed for add-ons made for other SuperTuxKart versions.
- Not every archive layout is recognized. Stick to the accepted layouts.
- Large mods need free space and time for staging.
- Symlink refusal and path checks are not a security sandbox. Install mods you trust.

An accurate description of what is supported: correctly packaged SuperTuxKart 1.5 kart and track-family add-ons, within these limits. It is not "any SuperTuxKart mod".

#### All scanned locations

You only need `/data/stk/mods` or a USB `mods` folder. For completeness, the importer scans these internal roots:

```text
/data/stk/mods
/data/stk/addons
/data/mods
/data/addons
/data/SuperTuxKart/mods
/data/supertuxkart/mods
/data/STK/mods
```

For USB it checks `/mnt/usb0` through `/mnt/usb7`. Under each drive it looks in `mods`, `Mods`, `MODS`, `stk-mods`, `addons`, `SuperTuxKart/mods`, `SuperTuxKart/addons`, `supertuxkart/mods`, `supertuxkart/addons`, `STK/mods`, `STK/addons`, `stk/addons`, and the corresponding `PS4/...` paths, plus `karts/` and `tracks/` collections. USB scanning is implemented, but the recorded positive test used an internal path, not USB.

Unpacked-folder import is also implemented. The recorded positive test used a ZIP.

#### Add-on list entries and dates

The Add-ons screen lists only entries known to the add-on manager, so copying files into the add-ons directory is not enough on its own. The port registers locally imported karts and track-family entries (tracks and arenas) that have no catalogue entry, so they show up. It also repairs missing or zero install dates on those local entries from the installed folder's modification time, so they should not display as 1970.

That date repair is implemented but not hardware-verified. Do not edit XML files to change dates.

#### Compatibility advice

- Prefer the curated catalogue for content that is known to work with SuperTuxKart 1.5.
- Use packages made for SuperTuxKart 1.5.
- Keep `kart.xml` or `track.xml` at the archive root or inside one clean top-level folder.
- Keep XML file names lowercase.
- When troubleshooting, add one mod at a time.
- To remove a bad import, delete its installed folder under `/data/stk/supertuxkart/addons/` and remove its line from `/data/stk/mods/.ps4-imported`. Do not delete all of `/data/stk` to fix one add-on.

---

## Caveats

This is the consolidated list. Each item is also covered where relevant above.

### Settings

- **Changing controls settings is untested.** Use the default controls.
- **Changing graphics settings is untested.** Do not change resolution, refresh rate, visual presets, or individual effects. Leave graphics settings alone for now. The settings screens may open, but applying and persisting changes is not part of the verified configuration. This is "untested", not "known broken".
- **Audio settings work.** Changing them was tested on the port; the new values take effect.

### Hardware and platform

- One console and firmware combination was tested: PS4 slim, firmware 13.52.
- Output was tested at 1920x1080 only. 4K, HDR, variable refresh, and other modes are not tested.
- Suspend/resume, rest mode, Remote Play, and external HDD installs are not tested.
- The benchmark result (31 FPS steady, 42 FPS typical) is from one PS4 slim at default settings. It is not a guarantee for other consoles, firmware, or settings.
- Long-session stability is untested.

### Gameplay

- Online and LAN multiplayer are not tested.
- Only two-player split-screen was tested. Three and four players are not tested.
- Only default controls on DualShock 4 were tested.
- Not every mode, track, arena, kart, or challenge was played.

### Add-ons

- **Uninstalling add-ons from the in-game menu does not work.** The menu returns an error instead of removing the add-on. Remove it manually: delete its folder under `/data/stk/supertuxkart/addons/` (under `karts/` or `tracks/`), then remove its line from `/data/stk/mods/.ps4-imported` if you kept the source.
- The curated menu was tested with one kart (Among Us). Other catalogue items are untested.
- Manual import was tested with one kart ZIP (`example_kart.zip`).
- Track, arena, and soccer import are implemented, not verified.
- Date repair for local add-ons is implemented, not verified.
- Same-name replacement and rollback, the root-XML ZIP fallback, unpacked-folder import, and USB scanning are implemented, not independently verified.
- The importer does not repair or complete broken mods.
- Mods that depend on missing assets can look wrong or fail.

### Implemented but not verified

These code paths exist in the release. They were not part of a recorded hardware test of the final package, so they are not listed under [What works](#what-works).

| Feature | State |
|---|---|
| Track add-on import | The importer accepts `track.xml` and routes it under `tracks/`. No recorded hardware test of a track ZIP. |
| Battle / CTF arena import | `track.xml` with `arena="Y"` is accepted. No recorded hardware test. |
| Soccer field import | `track.xml` with `soccer="Y"` is accepted. No recorded hardware test. |
| Local add-on date repair | Zero or missing dates are filled from the folder modification time. Not confirmed by screenshot or log. |
| PS4 system keyboard (IME) for text fields | Pads are paused while the IME is open. Not verified in the final test matrix. |
| Closing the game | A close-request path exists. A repeatable final-package shutdown test was not recorded. |
| 3 and 4 player split-screen | Four pad slots exist in code. Only two players were tested. |
| Same-name mod replacement and rollback | A staged replace exists. Not independently exercised on hardware. |
| Archive-name install directory for root-XML ZIPs | Implemented. Only the tested sample ZIP is confirmed. |
| Unpacked-folder import | Implemented. The positive test used a ZIP. |
| USB add-on scanning | Implemented. The positive test used an internal path. |

### Not claimed

- Sony approval, licensing, or official status.
- A formal audit or certification of the package.
- Save migration from every earlier build.
- That the final package was freshly reinstalled and every feature retested after the last documentation change.

### Withdrawn release

v0.10.0 is withdrawn. Do not use it.

---

## Troubleshooting

**The game says nothing for a while on first launch.**
Shader compilation. The cache is written to `/data/stk/cache` and reused later. Wait for it to finish.

**The second controller is not recognized in split-screen.**
Each player needs a separate signed-in PS4 user. Sign in both users on the PS4 user screen, connect both controllers, and relaunch the game.

**The Add-ons menu cannot show the catalogue or download.**
The curated path needs a working network connection and a reachable official add-on service. Check the console's connection. If the service is down, use the manual path for a mod you already have.

**A manual mod does not appear.**
Go through this list:

1. Is it a `.zip` or an unpacked folder? `.7z` and `.rar` are not read.
2. Is `kart.xml` or `track.xml` present, in lowercase, at the root or in one top-level folder, within 4 levels?
3. Did you copy it to a scan location such as `/data/stk/mods`?
4. If it is on USB, was the drive connected and mounted before you launched the game?
5. Did you restart the game after copying? The scan runs at startup.
6. Is it made for SuperTuxKart 1.5?

**A manual mod appears but looks wrong or fails.**
The mod may be missing assets or may target a different game version. The importer does not repair content.

**A mod was replaced unexpectedly.**
Add-ons are stored one directory per name. A changed source with the same name replaces the installed one.

**Uninstalling an add-on from the in-game menu fails.**
This is a known issue: the game returns an error instead of removing the add-on. Delete its folder under `/data/stk/supertuxkart/addons/` manually. If the original source is still in a scan location, also remove its line from `/data/stk/mods/.ps4-imported`, or the next launch can import it again.

**I changed a graphics or controls setting and something is off.**
Those settings are untested on this port. Revert to the defaults. If that does not help, a full reset of `/data/stk` clears settings but also deletes saves, add-ons, and the shader cache.

---

## Where data lives

| Path | Purpose |
|---|---|
| `/app0` | Packaged executable and game data. Read-only. |
| `/data/stk` | Writable root for saves and configuration. Persistent. |
| `/data/stk/cache` | Shader and pipeline cache. |
| `/data/stk/supertuxkart/addons` | Installed add-ons (`karts/`, `tracks/`). |
| `/data/stk/mods` | Default manual import location. |
| `/data/stk/mods/.ps4-imported` | Importer state file. |
| `/data/stk/config-0.10` | Configuration directory. |

Profiles, player data, and logs also live under `/data/stk`.

The port sets these locations at startup:

```text
SUPERTUXKART_DATADIR=/app0
SUPERTUXKART_SAVEDIR=/data/stk
XDG_DATA_HOME=/data/stk
MESA_SHADER_CACHE_DIR=/data/stk/cache
```

Saves are local to the console. There is no cloud sync. Moving saves between firmware versions or from other builds was not tested.

---

## Build from source

You do not need to build anything to play. The release package is the tested artifact. **A package you build yourself is a different file with a different hash and is not the tested v0.11.0 package.** Rebuilding can change bytes, timestamps, and package metadata.

This is not a one-line desktop build. It requires a homebrew PS4 toolchain and some familiarity with cross-compiling. [`ps4/PORTING.md`](ps4/PORTING.md) has the exact toolchain versions and prerequisites. Read it before changing anything.

### Prerequisites

- The `ps4-kit` build environment: clang-21 and `ld.lld-21` targeting PS4, a PS4 sysroot (OpenOrbis), and a patched Mesa/RADV for PS4. Other compilers are not claimed to work.
- The SDL2 port layer and the PS4 Vulkan graphics path.
- An environment script that exports `PS4_SYSROOT`, `OPENORBIS`, `PS4_MESA_BUILD`, and `PSB`.
- A checkout of `stk-assets` containing the full asset set, including `tracks/`. The default location is `../stk-assets` next to this repository.
- `sce_sys/about/right.sprx` for packaging.

### Standalone vs. developer data

`PS4_EXTERNAL_DATA=OFF` is required for the public package. It compiles `/app0` as the data root and packages the full assets. `PS4_EXTERNAL_DATA=ON` produces a developer build that reads data from `/data/stk/data` and is not a release configuration. `ps4/pkg.sh` requires `PS4_EXTERNAL_DATA=OFF`.

### Commands

After sourcing your toolchain environment script:

```bash
STK_VERSION=00.11 BUILD_DIR=/path/to/build bash ps4/build.sh
STK_VERSION=00.11 BUILD_DIR=/path/to/build bash ps4/pkg.sh
```

The tested package was built in a separate, space-free build directory outside the source tree because the source path contained spaces. `ps4/build.sh` uses `BUILD_DIR` when it is set and otherwise falls back to a space-free directory when the checkout path contains spaces.

`pkg.sh` stages the executable, data, and `sce_sys`, refuses an incomplete asset tree or the wrong `PS4_EXTERNAL_DATA` mode, and calls the kit's package builder. The release artifact exists under both its content-ID and friendly filenames as hard links to the same file. Publish one of them, not both.

### Key configuration

| Option | Value |
|---|---|
| `PS4_PORT` | `ON` |
| `CMAKE_BUILD_TYPE` | `Release` |
| `STK_VERSION` | `00.11` |
| `PS4_EXTERNAL_DATA` | `OFF` for the standalone package |
| `USE_WIIUSE` | `OFF` |
| `BUILD_RECORDER` | `OFF` |
| `USE_GLES2` | `OFF` |

Other options (bundled `enet`, `squish`, `mcqp`, and `sqlite3`, Discord RPC disabled, and so on) are set by the PS4 build scripts and documented in `ps4/PORTING.md`.

### Common mistakes

- **No `stk-assets/tracks`:** the standalone package is invalid.
- **Missing `right.sprx`:** packaging or validation fails.
- **`PS4_EXTERNAL_DATA=ON`:** produces a developer package that expects `/data/stk/data`. Do not distribute it.
- **Incomplete assets:** can lead to a black screen or missing-data failures.
- **Committing the package:** the output directory is gitignored. Release packages are GitHub Release assets, not repository files.

`ps4/deploy.sh`, `ps4/push.py`, `ps4/upload-data.py`, and `ps4/pull-logs.py` are developer test helpers for uploading builds and pulling logs from a test console. They are not part of the release package path.

---

## Technical notes

Short summary. Details are in the port documents linked below.

**Graphics.** SuperTuxKart's Vulkan renderer runs through a PS4 SDL2 Vulkan window-system layer and the console's VideoOut. The surface is 1920x1080. OpenGL ES and software rendering are not used. No claim is made about 4K, HDR, variable refresh, selectable resolutions, or visual presets. See [Performance](#performance) for the one measured frame-rate result.

**Shader cache.** The port uses a RADV shader and pipeline cache in `/data/stk/cache`, created at startup. Earlier builds recompiled shaders every launch and stalled; later launches reuse the cache.

**Audio.** SDL audio with SuperTuxKart's 44.1 kHz mix, a 48 kHz device path, and small buffers. Audio worked on the tested paths, and changing the audio settings was tested and takes effect. Surround output and other output devices are not claimed.

**Input.** The PS4 pad API feeds SuperTuxKart's input manager. The port tracks signed-in users, opens a pad per user, retries failed opens, and releases a pad when its user signs out. It suppresses the first-frame Cross press on the race screen so confirming a kart does not accelerate or fire an item. Pads pause while the system keyboard is open. The input layer has four pad slots, but only two players were tested.

**Threading.** PS4-specific wrappers cover the pthread and semaphore behavior the game needs.

**Other.** Discord rich presence is disabled. Optional tracing and watchdog logging exist for debugging and are not needed for play.

---

## Credits and license

- **SuperTuxKart** is developed by the SuperTuxKart team. This port is based on their source and assets. See [`README.stk.md`](README.stk.md) for the upstream readme.
- **License.** SuperTuxKart's code is GPLv3 and its assets carry mixed licenses. See [`COPYING`](COPYING) and the license files that ship with the assets. Add-ons carry their own licenses.
- **Port toolchain and components.** The ps4-kit environment, Mesa/RADV, SDL2, and OpenOrbis, with PS4-specific patches and glue code in the [`ps4/`](ps4/) directory.
- **Status.** This is an unofficial community port. It is not affiliated with or endorsed by Sony Interactive Entertainment or the SuperTuxKart team. "PlayStation" and "PS4" are trademarks of their owners.

---

## More documentation

| Document | Contents |
|---|---|
| [`RELEASE_NOTES_v0.11.0.md`](RELEASE_NOTES_v0.11.0.md) | The v0.11.0 release notes. |
| [`ps4/README.md`](ps4/README.md) | Short port overview: build, controls, add-ons, known issues. |
| [`ps4/PORTING.md`](ps4/PORTING.md) | Toolchain, CMake integration, Vulkan, SDL, audio, input, packaging, runtime environment. |
| [`ps4/PORT-NOTES.md`](ps4/PORT-NOTES.md) | Runtime history and evidence: present path, graphics debugging, shader cache, tutorial and title fixes, audio, input, add-ons, logs. |
| [`ps4/SDL_API_SURFACE.md`](ps4/SDL_API_SURFACE.md) | SDL and Vulkan window-system notes. |
| [`README.stk.md`](README.stk.md) | Upstream SuperTuxKart readme. |
| [`COPYING`](COPYING) | License notice. |

If a port document and this README disagree about what was tested, this README's [What works](#what-works) and [Caveats](#caveats) sections are the conservative reference.
