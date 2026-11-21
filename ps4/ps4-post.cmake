# SuperTuxKart PS4 port: link, runtime and eboot.
#
# Included at the end of CMakeLists.txt, after the supertuxkart executable
# exists. ps4_kit_app() must run after the target is defined; it adds the
# runtime objects, the linker wraps, and the <target>_eboot/eboot.bin step.

include("${PS4_KIT}/ps4-kit.cmake")

# SDL2's null video driver gives Irrlicht a window and an event queue but no
# Vulkan surface. These two files map the four SDL Vulkan calls onto the
# kit's headless-surface WSI, and the DualShock onto SDL key events.
target_sources(supertuxkart PRIVATE
    ${PROJECT_SOURCE_DIR}/ps4/ps4_sdl_vulkan.cpp
    ${PROJECT_SOURCE_DIR}/ps4/ps4_input.cpp
    ${PROJECT_SOURCE_DIR}/ps4/ps4_ime.cpp
    ${PROJECT_SOURCE_DIR}/ps4/ps4_threads.cpp
    ${PROJECT_SOURCE_DIR}/ps4/ps4_trace.cpp)
# Milestones at STK's main, SDL's init/window/first poll, and the watchdog stack dump that names
# where a black screen is stuck (hang.log).
foreach(fn main SDL_Init SDL_InitSubSystem SDL_TimerInit SDL_VideoInit SDL_CreateWindow SDL_PollEvent)
    target_link_options(supertuxkart PRIVATE --wrap=${fn})
endforeach()
# exit() runs the VideoOut handoff in ps4_trace.cpp before the process ends; the app's quit paths
# never reached ps4_kit_exit, and the shell rebooted when the game closed.
target_link_options(supertuxkart PRIVATE --wrap=exit)
# STK's edit boxes call these when they gain/lose focus; ps4_ime.cpp turns them
# into the system on-screen keyboard instead of silently doing nothing.
foreach(fn SDL_StartTextInput SDL_StopTextInput)
    target_link_options(supertuxkart PRIVATE --wrap=${fn})
endforeach()
# libkernel's pthread attribute objects come from a pool that can fail once the heap exists, and
# SDL, libc++ and Mesa all ignore the error and dereference the attribute. ps4_threads.cpp replaces
# a refused attribute and emulates recursive mutexes over plain ones; its probe logs the return
# codes to boot-trace.log. SDL itself is built without the attribute API (SDL_sysmutex.c).
foreach(fn
        pthread_mutexattr_init
        pthread_mutexattr_settype
        pthread_mutexattr_destroy
        pthread_mutex_init
        pthread_mutex_lock
        pthread_mutex_unlock
        pthread_mutex_trylock
        pthread_mutex_destroy)
    target_link_options(supertuxkart PRIVATE --wrap=${fn} --undefined=${fn})
endforeach()
foreach(fn
        SDL_Vulkan_GetVkGetInstanceProcAddr
        SDL_Vulkan_GetInstanceExtensions
        SDL_Vulkan_CreateSurface
        SDL_Vulkan_GetDrawableSize)
    target_link_options(supertuxkart PRIVATE --wrap=${fn})
endforeach()
# STK uses the plain target_link_libraries signature; the kit helper and this
# file follow it so CMake does not reject a mixed signature.
# There is no Discord client on a PS4: RichPresence opens a Unix socket that cannot exist, fails
# on every frame and starts a thread whose only work is that failing connection. The source's
# DISABLE_RPC path is the one the Switch build uses (empty update(), no socket, no thread); define
# it here so STK's source stays untouched.
target_compile_definitions(supertuxkart PRIVATE DISABLE_RPC)
# Release builds use the compile-time /app0 data root. Only the eboot-only
# development build opts into /data/stk. Keeping this out of a runtime
# existence check means a release package can never read the developer's
# external data tree, and uninstalling it removes the bundled assets.
if(PS4_EXTERNAL_DATA)
    target_compile_definitions(supertuxkart PRIVATE PS4_EXTERNAL_DATA=1)
endif()
# STK's Unix branch links ${FREETYPE_LIBRARIES}, which is empty with the
# sysroot's CMake package (it exports Freetype::Freetype instead). Add the
# archive explicitly; HarfBuzz's hb-ft glue needs these symbols.
target_link_libraries(supertuxkart "${PS4_FREETYPE_LIB}")
# SDL's PS4 audio backend is compiled into the sysroot's libSDL2.a and
# resolves its output calls against the system audio library.
target_link_libraries(supertuxkart -lScePad -lSceUserService -lSceAudioOut -lSceImeDialog -lSceSysmodule)

# Static RADV from the kit's Mesa build, plus the kit's presenter:
# VK_EXT_headless_surface + VK_KHR_swapchain on VideoOut.
# Presenting flips at 1920x1080; the system scales for a 720p TV.
ps4_kit_link_mesa(supertuxkart)

# ps4-kit runtime (dlmalloc heap, stdio lock, *at path layer, crash log,
# resolver), the app-only linker wraps, and the fake-SELF eboot.bin.
ps4_kit_app(supertuxkart)

# STK defines SUPERTUXKART_DATADIR from STK_INSTALL_DATA_DIR (/app0), which
# ps4-platform.cmake sets. pkg.sh stages the data at /app0/data.

# Keep the crash log useful: Ps4-Kit symbolizes against the exact oelf.
set_target_properties(supertuxkart PROPERTIES
    ENABLE_EXPORTS OFF
    POSITION_INDEPENDENT_CODE ON)
