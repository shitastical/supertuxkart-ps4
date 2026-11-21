// Startup milestones and a watchdog stack dump for the PS4 port.
//
// The port shows a black screen and stays there: no crash, one core busy. Nothing in the eboot says
// where it is, and the kit's Vulkan WSI step trace stays empty (it never reaches Vulkan), so this
// file puts the startup handoffs into boot-trace.log and, from inside the running process, dumps
// every thread's stack to hang.log - the same dump the crash handler writes, without the crash.
//
// The dump is free when the app runs: it only reads stacks and appends a file. It stops after the
// first 90 seconds; the app is not held back or interrupted.
//
// It is off by default now that the game gets past startup: the 90 second dump lands while the
// player is in the menus or loading a race, and the old trace ended right after it with no exit
// line and no crash.log (the process was gone). Enable it only to chase a startup hang, with
// STK_PS4_WATCHDOG=1 in the environment.

#include "ps4_kit_diag.h"

#include <SDL.h>

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

void milestone(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

void milestone(const char* fmt, ...) {
    char text[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof text, fmt, args);
    va_end(args);
    ps4_boot_trace(text);
}

// One dump per milestone: the first names the call the app sits in; a later one that differs shows
// progress, an identical one shows a loop.
void dumpStacks(const char* when) {
    char path[256];
    snprintf(path, sizeof path, "%s/hang.log", ps4_diag_data_dir());
    char reason[192];
    snprintf(reason, sizeof reason, "watchdog %s after start (no frame drawn yet)", when);
    ps4_dump_thread_stacks(path, reason);
}

void* watchThread(void*) {
    sleep(12);
    dumpStacks("12 s");
    sleep(13);
    dumpStacks("25 s");
    sleep(20);
    dumpStacks("45 s");
    sleep(45);
    dumpStacks("90 s");
    milestone("port: watchdog finished its stack dumps (the app is still running)");
    return nullptr;
}

}  // namespace

// SDL_INIT_TIMER|SDL_INIT_VIDEO is the pair that fails. SDL_InitSubSystem calls these two, so
// their results (and SDL's error for each) say which half is broken: the timer thread, or video.
extern "C" int __real_SDL_TimerInit(void);
extern "C" int __wrap_SDL_TimerInit(void) {
    static unsigned int calls;
    const int result = __real_SDL_TimerInit();
    if (calls++ < 5)
        milestone("port: SDL_TimerInit -> %d: \"%s\"", result, result < 0 ? SDL_GetError() : "ok");
    return result;
}

extern "C" int __real_SDL_VideoInit(const char* driver_name);
extern "C" int __wrap_SDL_VideoInit(const char* driver_name) {
    static unsigned int calls;
    const int result = __real_SDL_VideoInit(driver_name);
    if (calls++ < 5)
        milestone("port: SDL_VideoInit(\"%s\") -> %d: \"%s\"", driver_name ? driver_name : "auto",
                  result, result < 0 ? SDL_GetError() : "ok");
    return result;
}

// The kit's runtime already provides __wrap_exit (ps4_kit_exit: hand VideoOut back, then ask the
// shell to close the app). --wrap=exit in ps4-post.cmake is what finally routes STK's exit(0)
// through it, so a quit no longer leaves the display registered and reboots SceShellUI.
extern "C" void ps4kit_video_shutdown(void) __attribute__((weak));

// STK's entry point (the C library calls main through a relocation, so --wrap=main applies).
extern "C" int __real_main(int argc, char** argv);
extern "C" int __wrap_main(int argc, char** argv) {
    milestone("port: STK main() entered (argc=%d)", argc);
    // Diagnostic switch on the console's data partition: the kit's hot-spot
    // profiler samples this thread every 20 ms and writes profile.log in the
    // kit's data dir every 10 s. Off unless the marker exists, so a normal
    // launch pays nothing. Sampling covers initRest() and the frame loop.
    if (access("/data/stk/profile-on", F_OK) == 0) {
        ps4_sampler_register_thread("main");
        ps4_hot_profiler_set_enabled(1);
        milestone("port: hot-spot profiler enabled by /data/stk/profile-on");
    }
    const int result = __real_main(argc, argv);
    milestone("port: STK main() returned %d", result);
    if (ps4kit_video_shutdown)
        ps4kit_video_shutdown();
    return result;
}

// SDL initialisation, the window and the first event poll: Irrlicht's device creation calls these
// in this order before it asks for Vulkan.
extern "C" int __real_SDL_Init(Uint32 flags);
extern "C" int __wrap_SDL_Init(Uint32 flags) {
    milestone("port: SDL_Init(0x%x)", static_cast<unsigned>(flags));
    const int result = __real_SDL_Init(flags);
    milestone("port: SDL_Init -> %d", result);
    return result;
}

// SDL's subsystems are what STK creates first, and a failure here is what keeps the app on a black
// screen. The error string names the failing piece (timer thread, semaphore, video driver).
// A failing init makes Irrlicht retry the whole device thousands of times a second, so the first
// few failures are logged in full and the rest are counted: the trace must not become the load.
extern "C" int __real_SDL_InitSubSystem(Uint32 flags);
extern "C" int __wrap_SDL_InitSubSystem(Uint32 flags) {
    static unsigned int calls;
    static unsigned int failures;
    ++calls;
    const int result = __real_SDL_InitSubSystem(flags);
    if (result < 0) {
        if (failures < 5 || failures % 2000 == 0)
            milestone("port: SDL_InitSubSystem(0x%x) -> -1: \"%s\" (call %u, failure %u)",
                      static_cast<unsigned>(flags), SDL_GetError(), calls, failures + 1);
        ++failures;
    } else if (failures < 5) {
        milestone("port: SDL_InitSubSystem(0x%x) -> 0", static_cast<unsigned>(flags));
    }
    return result;
}

extern "C" SDL_Window* __real_SDL_CreateWindow(const char* title, int x, int y, int width, int height,
                                               Uint32 flags);
extern "C" SDL_Window* __wrap_SDL_CreateWindow(const char* title, int x, int y, int width, int height,
                                               Uint32 flags) {
    // The dummy driver has no Vulkan_CreateSurface or Vulkan_LoadLibrary hook, so SDL_CreateWindow
    // rejects SDL_WINDOW_VULKAN before it ever asks the driver for a window ("Vulkan support is
    // either not configured in SDL or not available in current SDL video driver"). This port does
    // not use SDL's Vulkan path: every SDL_Vulkan_* call STK makes is wrapped in
    // ps4_sdl_vulkan.cpp and goes to ps4-kit's WSI, so the window itself does not need the flag.
    // Drop it for the dummy driver only.
    Uint32 call_flags = flags;
    const char* driver = SDL_GetCurrentVideoDriver();
    if ((flags & SDL_WINDOW_VULKAN) && driver && strcmp(driver, "dummy") == 0)
        call_flags &= ~SDL_WINDOW_VULKAN;

    static unsigned int calls;
    static unsigned int failures;
    ++calls;
    milestone("port: SDL_CreateWindow(\"%s\", %dx%d, flags 0x%x -> 0x%x, driver %s)",
              title ? title : "", width, height, static_cast<unsigned>(flags),
              static_cast<unsigned>(call_flags), driver ? driver : "none");
    SDL_Window* window = __real_SDL_CreateWindow(title, x, y, width, height, call_flags);
    if (!window) {
        if (failures < 5 || failures % 2000 == 0)
            milestone("port: SDL_CreateWindow -> NULL: \"%s\" (call %u, failure %u)", SDL_GetError(),
                      calls, failures + 1);
        ++failures;
    } else if (failures < 5) {
        milestone("port: SDL_CreateWindow -> %p", static_cast<void*>(window));
    }
    return window;
}

extern "C" int __real_SDL_PollEvent(SDL_Event* event);
extern "C" void ps4_ime_poll(void);
extern "C" int __wrap_SDL_PollEvent(SDL_Event* event) {
    static int calls;
    ps4_ime_poll();
    const int result = __real_SDL_PollEvent(event);
    if (calls++ == 0)
        milestone("port: first SDL_PollEvent -> %d (event loop reached)", result);
    return result;
}

// After the kit's own constructors (heap, threads, trace), before STK's static initialisers.
__attribute__((constructor(210))) static void ps4_trace_start() {
    // STK's log file is a FILE* of its own (Log::m_file_stdout), not fd 1. On the console fd 1 is
    // the launcher's stdout and can be a pipe nobody drains: a printf from the texture loader
    // threads then blocks forever once the pipe fills, while those threads hold texture locks, and
    // the main thread spins in waitImageView - a black screen. Diagnostics belong in boot-trace.log
    // and hang.log, so keep fd 1 inert and no stray printf can ever freeze the game.
    freopen("/dev/null", "w", stdout);

    // This SDL's dummy video driver is not auto-selected: SDL_nullvideo.c's DUMMY_Available() only
    // reports true when SDL_HINT_VIDEODRIVER names it. With no driver SDL_VideoInit(NULL) fails
    // with "No available video device" and Irrlicht retries device creation forever - the black
    // screen. The port needs no real window (Vulkan presents through ps4_vk_wsi), so dummy is
    // exactly the driver this build should use. The environment copy covers SDL builds that read
    // the variable directly.
    SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    // This must be a build-time choice, not a runtime existence check: a release package has its
    // assets in /app0/data and must never read a developer's /data/stk tree, while an eboot-only
    // development package has no /app0/data and reads the uploaded /data/stk/data. PS4_EXTERNAL_DATA
    // is set only by the dev build (ps4/deploy.sh).
#ifdef PS4_EXTERNAL_DATA
    const char* const data_root = "/data/stk";
#else
    const char* const data_root = "/app0";
#endif
    // Pin the root even for the release, where it matches STK's compile-time default: without the
    // variable FileManager probes ./data, ../data, ../../data and the executable's ../data relative
    // to the launcher's working directory before falling back. A release launched from a directory
    // that happens to contain a developer tree could otherwise load /data/stk instead of /app0.
    setenv("SUPERTUXKART_DATADIR", data_root, 1);
    // Without this, FileManager falls back to $HOME/.config (unwritable on the console), so the
    // player profile, users.xml/players.xml and stdout.log were never persisted. /data/stk is
    // writable and survives an uninstall; the bundled game assets in /app0 do not.
    setenv("SUPERTUXKART_SAVEDIR", "/data/stk", 1);
    // Puts STK's add-ons in /data/stk/supertuxkart/addons instead of the
    // unwritable ./supertuxkart/addons inside /app0.  Online add-on downloads
    // and the drag-and-drop mod importer both need a persistent writable path.
    setenv("XDG_DATA_HOME", "/data/stk", 1);
    // RADV's shader/pipeline disk cache is compiled in (the kit's gpu/mesa-patches 0023-0025 make
    // it work on the PS4), but Mesa only enables it when a folder is named: otherwise it looks for
    // $XDG_CACHE_HOME or $HOME/.cache, neither of which exists on the console. Without it every
    // launch recompiled every pipeline in NIR/ACO on the main thread - 500 ms frames and 4 fps,
    // measured by the hot-spot profiler - and the tutorial's hundreds of new pipelines stalled the
    // frame loop while a race loaded. /data/stk survives a package reinstall like the assets.
    mkdir("/data/stk/cache", 0777);
    setenv("MESA_SHADER_CACHE_DIR", "/data/stk/cache", 1);
    milestone("port: video driver dummy, data root %s, save dir /data/stk, "
              "addons /data/stk/supertuxkart/addons, RADV shader cache "
              "/data/stk/cache", data_root);
    // Either the environment or a marker file on the data partition switches the dump on, so a
    // console with no shell (this port has only FTP and klog) can ask for stack dumps between
    // installs: create /data/stk/watchdog-on and relaunch.
    const bool watchdog = getenv("STK_PS4_WATCHDOG") != nullptr ||
                          access("/data/stk/watchdog-on", F_OK) == 0;
    if (watchdog) {
        pthread_t thread;
        if (pthread_create(&thread, nullptr, watchThread, nullptr) == 0) {
            pthread_detach(thread);
            milestone("port: watchdog enabled (stack dumps at 12/25/45/90 s)");
        }
    }
}
