# SuperTuxKart on PS4: runtime notes and lessons

`PORTING.md` covers how the port is built and wired. This file covers how it
**runs**: the present path, the failures that took the longest to find, the
evidence that identified each one, and the rules that keep them from coming
back. It is written for whoever touches this port (or the next one on
ps4-kit) next.

Status as of 2026-11-20: **playable on retail hardware** — the menus render and
the tutorial loads. The close path below is the kit's shutdown path; record a
final close test on the next run. Installing and launching are manual XMB
steps; nothing here is a hardware claim beyond what was actually run.

## 1. How a frame reaches the screen

```
STK frame
 └─ GEVulkanDriver::endScene()
     ├─ resets the frame fence, submits the frame's command buffers
     └─ waits that fence (2 s bound; on PS4 timeout it skips the frame)
 └─ vkQueuePresentKHR → ps4-kit WSI (gpu/ps4_vk_wsi.cpp)
     ├─ waits the submitted frame's fence, capped at 200 ms
     │    └─ not signalled? log, skip the flip, return
     ├─ sceVideoOutSubmitFlip(handle, buffer, mode, flipArg)
     └─ no submitDone here — never here
 └─ next acquire / present
     └─ sceVideoOutGetFlipStatus().flipArg advanced
         └─ ac_ps4_flip_made(flipArg) → sceGnmSubmitDone once for that flip
```

Three invariants make this safe:

1. **`sceGnmSubmitDone` is a promise, not a notification.** It tells the
   kernel the frame's work is in and the GPU can fall idle; the kernel then
   gives it 500 ms. Calling it while the frame or its texture uploads are
   queued turns a busy GPU into `GPU_FAULT_SUBMITDONE_TIMEOUT_IN_SUSPEND_ASYNC`
   after 500 ms.
2. **Only VideoOut's `flipArg` proves a frame ended.** Not the submit call,
   not a timer, not "the queue looks empty". The kit's
   `ac_ps4_flip_made(int64_t)` accepts only a strictly newer flip number; a
   repeated or stale number is ignored. The old `ac_ps4_submit_done()` is an
   inert symbol that logs once, so a port cannot make the premature promise
   even if it still calls it.
3. **Every wait in the frame path is bounded.** A wait with no timeout is a
   freeze waiting for a reason. Present caps the fence wait at 200 ms and
   skips the flip on timeout; GE's own fence wait is 2 s. Nothing calls
   `vkDeviceWaitIdle` from a timeout on PS4.

## 2. The two failures that mattered

### 2.1 One frame per second, with the GPU mostly idle

The menus were visible but ran at 1.0 flips/s. The WSI stats line was:

```
ps4kit/wsi: 1.0 flips/s, longest frame 1029 ms, acquire waits 0 ms/s,
            0 timeouts, flipArg 84 of 86 submitted
```

Both `vkAcquireNextImageKHR` and `sceVideoOutSubmitFlip` took ~512 ms each.
That is not render cost and not a slow CPU: 512 ms is the kernel's 500 ms
`submitDone` deadline plus overhead, hit **every frame**.

Cause: the kit's CPU present path called `ac_ps4_submit_done()` *before*
`sceVideoOutSubmitFlip`. Every present promised an idle GPU that was still
drawing, so every present waited out the deadline. The fix was to delete that
call: present submits the flip, and the next `flipArg` advance calls
submitDone.

### 2.2 The tutorial froze and the console UI restarted

Loading the tutorial stopped at one submission. `gpu-hang.txt`:

```
waiting for 632, GPU completed 631, submitted 632
```

All GPU units were Idle. The app had not segfaulted: the kernel log showed a
system coredump, `reason=0xa0d0c007`, `KillApp(STKR00001)`, and SceShellUI
rebooting. The submit thread was stuck behind the same broken promise, and
present was waiting for a fence that could no longer signal.

Two fixes, both needed:

- submitDone only when VideoOut reports the flip made (the guarded API above);
- the frame-fence wait in present is capped at 200 ms and a timed-out frame is
  **not** flipped. Before this, present waited the fence with no bound, so a
  kernel that stopped completing submissions froze the app outright.

The previous GPU-side flip experiment (`ac_ps4_submit_flip`) is not the path
we use. It produced unrelated textures being replaced on screen: the flip's
command stream and the app's draws were not isolated the way the CPU flip
after a completed frame is. The present path is deliberately a plain
`sceVideoOutSubmitFlip` after the GPU frame is complete.

### 2.3 The close path

`__wrap_exit` (kit runtime) routes every exit through `ps4_kit_exit`, which
calls `ps4kit_video_shutdown()` before asking the shell to close the app. The
port also calls it after `__real_main` returns; the function is idempotent.
It waits up to 2 s for the last submitted flip, calls
`ac_ps4_flip_made(lastFlipArg)`, unregisters every buffer bank, and closes the
VideoOut handle with retries.

The old failure: a process that died with flips queued or buffers registered
left the display owned by a dead app; the shell restarted to recover. Never
force the console off while the app owns VideoOut — close with the PS button's
**Close Application**, or the in-game Quit.

## 3. Build, package, install

The build tree is outside the repository because the path contains a space:

```sh
source ~/cbfd-build/env.sh
cd /tmp/stk-ps4-build
ninja bin/supertuxkart supertuxkart_eboot
```

Release package (self-contained, assets in `/app0/data`):

```sh
cd /path/to/stk-code    # may contain spaces; BUILD_DIR keeps the build elsewhere
PS4_EXTERNAL_DATA=OFF BUILD_DIR="$HOME/stk-ps4-release" STK_VERSION=00.11 \
  bash ps4/build.sh
PS4_EXTERNAL_DATA=OFF BUILD_DIR="$HOME/stk-ps4-release" STK_VERSION=00.11 \
  bash ps4/pkg.sh
```

Development package (eboot only, assets in `/data/stk/data`):

```sh
PS4_EXTERNAL_DATA=ON BUILD_DIR="$HOME/stk-ps4-dev" STK_VERSION=00.01 \
  bash ps4/build.sh
BUILD_DIR="$HOME/stk-ps4-dev" STK_VERSION=00.01 STK_PKG_EBOOT_ONLY=1 \
  bash ps4/pkg.sh
```

`pkg.sh` checks the build cache: it rejects a full package built with
external data and an eboot-only package built without it. Both package types
include `sce_sys/about/right.sprx`; a fresh console can refuse a package that
omits it. A full package also fails rather than shipping with no `tracks/`.
The release binary pins `SUPERTUXKART_DATADIR=/app0`, so STK's
working-directory probes cannot pick up a `/data/stk` tree on a console that
has one; the dev binary pins `/data/stk`.

Upload and verify the size before installing:

```sh
curl -sS -T out/IV0000-STKR00001_00-STKPORT000000000.pkg \
  ftp://192.168.0.3:2121/data/pkg/IV0000-STKR00001_00-STKPORT000000000.pkg
curl -sS -I ftp://192.168.0.3:2121/data/pkg/IV0000-STKR00001_00-STKPORT000000000.pkg \
  | grep -i content-length
```

Install from `/data/pkg` with the XMB package installer (uninstall the old
`STKR00001` first; the installer refuses an already-installed content ID).
The release reads assets from `/app0/data` and reads/writes config in
`/data/stk/config-0.10/config.xml`; the development build reads assets from
`/data/stk/data`. The default renderer is Vulkan, `max_fps=120`,
`swap-interval-vsync=1`. Uninstall removes the `/app0` assets; the saves and
shader cache in `/data/stk` remain, because the package format has no
uninstall hook for that writable directory.

## 4. Logs: what to pull and what to read first

`python3 ps4/pull-logs.py` copies everything to `/tmp/stklogs-<utc>/`:

| File | What it answers |
|---|---|
| `stdout.log` (+ `.1`–`.3`) | STK's own log, WSI stats, shader/pipeline warnings |
| `boot-trace.log` | milestones and WSI step trace; gaps = no present |
| `hang.log` | watchdog thread stacks at 12/25/45/90 s |
| `crash.log` | app crash record — **check it is fresh** (see below) |
| `gpu-hang.txt` | RADV: waiting/completed submissions, GPU unit state |
| `klog.txt` | kernel's view: submitDone faults, coredump, KillApp, shell reboot |

Marker files on the console: `/data/stk/watchdog-on` enables stack dumps;
`/data/stk/profile-on` used to enable the profiler (it sampled every 20 ms and
never wrote a log, so it was removed). The watchdog marker stays.

**`crash.log` can lie.** During this bring-up it kept the same signature
across runs (signal 11, fault `0x254e0d7`, thread 100981, identical
registers), which is the signature of one old crash, not the current run. A
kernel coredump/kill is not an STK segfault: the klog says who killed the app,
and `crash.log` will not. Compare the fault address and thread id against the
run's timestamps before believing it.

## 5. Symptom → cause → first check

| Symptom | Likely cause | Check first |
|---|---|---|
| ~1 fps, every frame ~512 ms | submitDone while the frame is queued | WSI stats: flips/s, acquire waits, timeouts |
| Freeze on load, GPU units idle | same, plus an unbounded fence wait | `gpu-hang.txt` "waiting for N, completed N-1" |
| Console UI restarts after a freeze | kernel coredump after submitDone timeouts | `klog.txt`: `GPU_FAULT_SUBMITDONE`, `KillApp` |
| Black screen after closing | VideoOut buffers/handle not released | `boot-trace.log` shutdown lines; close path |
| Black screen at boot | SDL video/init retry loop | `boot-trace.log`; `SDL_GetError()` text |
| Crash in `SDL_CreateMutex`/libkernel | pthread attribute pool | kit's pthread wrappers |
| Textures replaced at random | deferred free retired too early, or GPU-side flip | see `ps4_defer` and §2.1 of the kit docs |
| Everything looks fine but nothing shows | presenting to the wrong buffer / no flip | WSI step trace: flip lines and flipArg |

## 6. The diagnostic method that worked

- **Instrument the boundary first.** The WSI already logs flips/s, longest
  frame, acquire wait time, timeouts and `flipArg` submitted/made. That line
  turned "the menus are laggy" into "1.0 flips/s, 512 ms per acquire and flip"
  in one run.
- **Separate render cost, submission and display.** `gpu-hang.txt` said the
  GPU had completed 631 and was idle while the app waited for 632. That rules
  out the renderer and points at the submit thread/kernel deadline.
- **Treat the kernel log as authoritative for submitDone.** The app's own
  stacks showed a wait; the klog showed the 500 ms promise being broken.
- **Change one thing per build.** The final fix was two changes (submitDone
  placement, bounded fence wait) that address one contract; they were packaged
  together and confirmed on hardware. Earlier, several plausible fixes at once
  made it impossible to tell which one mattered.
- **Never accept a wait without a timeout** in code that can run while the
  shell owns the display. The freeze was a missing bound, not a slow operation.
- **Keep the last known-good package.** Each iteration replaces the installed
  app; knowing the previous hash/size is what makes a rollback possible.

## 7. Next levers and implementation notes

- **Fixed pipeline.** STK logs
  `[warn] irr_driver: Using the fixed pipeline (old GPU, or shaders disabled
  in options)`; `CVS->isGLSL()` is false and there is no `glsl` key in the
  config. The playable build is therefore on the older fixed-function path.
  This is the first thing to measure if the frame rate is not where it should
  be: compare the GLSL path before changing anything else.
- **Heap arena cap.** At ~91 s one log line appeared:
  `[ps4] heap: automatic 4 MiB arena 12 of 12 for a thread that waited 32
  times for a heap lock`. Twelve 4 MiB arenas is the cap; a thread waiting 32
  times is contention, not a crash. It did not break the playable build, but
  it is the next thing to look at for loading hitches (raise the arena count
  or reduce the lock hold time in ps4-kit's heap).
- **Audio path (resolved).** MojoAL→SDL→SceAudioOut is linked and decoding.
  The SDL PS4 backend uses a 1024-frame (21 ms) buffer. MojoAL mixes at
  44.1 kHz (`lib/mojoal/mojoal.c`), the rate of nearly all music and sfx, so
  those sources reach the mixer without a per-source `SDL_AudioStream` that
  allocates and resamples inside the audio callback; SDL converts the finished
  mix once at the 48 kHz device boundary. The music stream queues four
  quarter-second buffers instead of two (`src/audio/music_ogg.*`), so a slow
  decode cannot starve the source and restart it mid-buffer. The old slowdown
  and crackle are no longer reproducible in the current build.
- **Control prompts (2026-11-20).** STK worded its prompts from the keyboard
  config because that is the device the pad thread drives. `KeyboardConfig::
  getBindingAsString` now has a `__PS4__` branch that returns the pad control
  for every action, so the tutorial, the input options and the spectate hints
  name the pad instead of Up/Space/Backspace/Return. The "press a key" dialog
  and the input-screen help line are reworded on PS4 the same way.
- **Button icons in prompts (2026-11-20).** The prompts draw the real pad
  control instead of words: `data/ttf/DS4Prompts.ttf` maps U+E000–U+E009 to
  Cross, Circle, Square, Triangle, L1, L2, R1, R2, Options and the left
  stick. The face
  buttons are the pack's discs with the symbol knocked out; the shoulders are
  keycaps whose letters are solid, because the pack's label was only about
  4 px tall at prompt size and disappeared. The font is appended to the
  `normal-ttf` fallback list in both `stk_config.xml` copies, and the `__PS4__`
  branch returns those codepoints, so the glyph is tinted with the prompt text
  and needs no per-widget image. Steering is one control, so both directions
  return the single left-stick disc (U+E009); printing a left-stick-left
  and a left-stick-right icon side by side said the same thing twice. The tutorial strings lost their literal `<%s>`
  brackets (they wrapped the icon) and the steering sentence mentions the
  stick once; the network spectate hint lost the same brackets.
  `stk_config.xml` loads the font through STK's normal asset lookup, so the
  release package carries it in `/app0/data/ttf/` and the dev tree in
  `/data/stk/data/ttf/`.
- **First-run login (2026-11-20).** With no local profile yet, backing out of
  the account login screen popped the login and the player-creation screen
  below it, which empties the menu stack and aborts the game. On PS4 that
  branch now pops only the login screen, so Circle returns to player creation
  and the application stays up; it is closed with PS > Close Application.
- **Controls (2026-11-20b).** The pad layout is now Mario Kart 8 Deluxe's:
  Cross accelerates, Circle brakes/reverses, the left stick steers, R2 fires,
  L2 drifts, L1 nitros, Square looks back, Triangle rescues, Options pauses.
  Pushing the stick forward no longer accelerates; only Cross does, and the
  stick is steering only. Prompts follow the same table.
- **PS4 gamepads and split-screen (2026-11-20d).** The old pad thread emulated
  one keyboard, so every controller collapsed into player 1. It now polls
  ScePad and queues native gamepad events for dispatch on the main thread;
  each pad is registered as a separate `GamePadDevice` with a stable non-SDL
  irrlicht ID. The first split-screen build paired logical slot N with ScePad
  index N, so slot 1 asked the first user for a second pad at index 1. A second
  signed-in user's standard controller is index 0 for that user, so it was
  never opened. The opener now tries every logged-in user's index-0 pad first,
  then the extra indices, and adopts an already-open handle through
  `scePadGetHandle` instead of failing forever. Registration waits for a
  successful `scePadReadState` with `data.connected` set, so an empty slot
  cannot become a phantom controller; the login list and open/read outcomes
  are written to `boot-trace.log`. The left stick steers with analog values in
  a race, and `DETECT_NEW` can assign a second pad to split-screen. Menus still
  use full stick deflection for quick navigation. The IME path is preserved:
  Cross on a focused text field opens the system keyboard and that press is
  suppressed so it cannot also select the widget behind the dialog. The
  split-screen prompt is just `Press [Cross] to join the game`.
- **Text input (2026-11-21).** The PS4 IME now pre-fills every focused text
  box, including passwords, and writes the accepted result back with
  `setText` so it replaces the old value instead of appending. The focused
  widget is cached while the system dialog is open, so losing GUI focus no
  longer discards the result. Password fields use a centred bullet mask on
  PS4; the asterisk was not visible in the single-line edit box.
- **Profiling.** The marker file was removed because it enabled a sampler that
  never wrote output. If profiling is needed again, add the writer first, then
  the marker.

## 8. Files and contracts

STK side:

| File | Role |
|---|---|
| `ps4/README.md` | build/package overview |
| `ps4/PORTING.md` | integration dossier |
| `ps4/ps4-platform.cmake`, `ps4/ps4-post.cmake` | CMake seam, kit link, `--wrap=exit` |
| `ps4/ps4_sdl_vulkan.cpp` | SDL Vulkan surface via the kit WSI |
| `ps4/ps4_input.cpp`, `ps4/ps4_ime.cpp` | ScePad mapping, IME dialog |
| `ps4/ps4_threads.cpp` | pthread wrappers the kit needs |
| `ps4/ps4_trace.cpp` | milestone logging, shutdown after `__real_main` |

Kit side:

| File | Role |
|---|---|
| `gpu/ps4_vk_wsi.cpp` | swapchain, present, acquire, flip accounting, shutdown |
| `gpu/mesa-patches/` | PS4 RADV series; `ac_ps4_drm.c` owns submissions and `ac_ps4_flip_made` |
| `runtime/ps4_runtime.cpp` | `ps4_kit_exit`, the only exit path |
| `docs/gpu-submit-and-videout.md` | the submitDone/VideoOut contract and evidence |
| `docs/bring-up-and-iteration.md` | the earlier black-screen/init failures |

Rules, in one place:

1. `sceGnmSubmitDone` only through `ac_ps4_flip_made(flipArg)`, only after
   `sceVideoOutGetFlipStatus` shows that flip made.
2. No unbounded fence or flip wait in the frame path.
3. No `vkDeviceWaitIdle` from a timeout on PS4.
4. Release VideoOut before exit; never force the console off while it is owned.
5. Only claim hardware results that were actually run.
