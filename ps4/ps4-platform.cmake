# SuperTuxKart PS4 port: configure-time integration.
#
# Included from the top of CMakeLists.txt when the build is configured with
# -DPS4_PORT=ON and the ps4-kit toolchain:
#
#   cmake -DCMAKE_TOOLCHAIN_FILE=$PS4_KIT/toolchain/ps4-kit-toolchain.cmake \
#         -DPS4_PORT=ON ...
#
# The toolchain file already defines the target (x86_64-pc-freebsd12-elf),
# clang 21 + libc++ 21, __PS4__, and puts PS4_SYSROOT ahead of PacBrew's libs.
# This file only chooses STK's platform options and locates the port libraries.

if(NOT DEFINED PS4_KIT)
    if(DEFINED ENV{PS4_KIT})
        set(PS4_KIT "$ENV{PS4_KIT}")
    else()
        set(PS4_KIT "$ENV{HOME}/ps4-kit")
    endif()
endif()
if(NOT EXISTS "${PS4_KIT}/ps4-kit.cmake")
    message(FATAL_ERROR
        "PS4_KIT='${PS4_KIT}' does not contain ps4-kit.cmake.\n"
        "Clone https://github.com/iHaiDeeZ/ps4-kit and run its "
        "scripts/seed-build-tree.sh once (it creates ~/ps4kit-build; that "
        "needs PacBrew's OpenOrbis and a seed sysroot).")
endif()

if(NOT DEFINED PS4_SYSROOT)
    if(DEFINED ENV{PS4_SYSROOT})
        set(PS4_SYSROOT "$ENV{PS4_SYSROOT}")
    else()
        set(PS4_SYSROOT "$ENV{HOME}/ps4kit-build/sysroot")
    endif()
endif()
if(NOT EXISTS "${PS4_SYSROOT}")
    message(FATAL_ERROR
        "PS4_SYSROOT='${PS4_SYSROOT}' does not exist.\n"
        "Run ${PS4_KIT}/scripts/seed-build-tree.sh and "
        "${PS4_KIT}/gpu/build-mesa.sh first.")
endif()

# --- Platform switches ------------------------------------------------------
# Set as cache entries before STK's option() calls: option() leaves an
# existing cache entry alone, so these become the PS4 defaults.
set(PS4_PORT              ON  CACHE BOOL "" FORCE)
set(USE_GLES2             OFF CACHE BOOL "" FORCE) # Vulkan is the renderer here
set(BUILD_RECORDER        OFF CACHE BOOL "" FORCE) # needs libopenglrecorder + GL
set(USE_WIIUSE            OFF CACHE BOOL "" FORCE) # needs BlueZ
set(USE_SYSTEM_WIIUSE     OFF CACHE BOOL "" FORCE)
set(USE_SQLITE3           OFF CACHE BOOL "" FORCE) # no sqlite3 in the sysroot
set(USE_MOJOAL            ON  CACHE BOOL "" FORCE) # bundled software OpenAL
set(USE_SYSTEM_ENET       OFF CACHE BOOL "" FORCE) # use the bundled enet
set(USE_SYSTEM_SQUISH     OFF CACHE BOOL "" FORCE)
set(USE_SYSTEM_MCPP       OFF CACHE BOOL "" FORCE)
set(USE_CRYPTO_OPENSSL    OFF CACHE BOOL "" FORCE) # mbedtls is the small build
set(USE_DNS_C             OFF CACHE BOOL "" FORCE)
set(USE_IPV6              ON  CACHE BOOL "" FORCE) # ENet's ENetIP is a struct
set(CHECK_ASSETS          OFF CACHE BOOL "" FORCE) # assets are staged, not in the tree
# pkg.sh stages data at /app0/data. Defining STK_INSTALL_DATA_DIR here makes
# STK emit SUPERTUXKART_DATADIR="/app0" itself instead of the default
# /usr/local/share/supertuxkart (and avoids a duplicate macro definition).
set(STK_INSTALL_DATA_DIR  "/app0" CACHE STRING "" FORCE)
set(SERVER_ONLY           OFF CACHE BOOL "" FORCE)

# ps4-kit's runtime pulls libsmb2/libnfs for /net shares; STK does not need it.
set(PS4_KIT_NETFS         OFF CACHE BOOL "" FORCE)

# X11 cannot exist here. OpenGL is left enabled at compile time only so the
# Irrlicht SDL device keeps its full translation unit; the Vulkan device is
# what actually runs.
add_definitions(-DNO_IRR_COMPILE_WITH_X11_)

# Vulkan, SDL2 and the other port libraries live in the project sysroot, not
# in OpenOrbis' libc sysroot. Put it first so <vulkan/vulkan.h> and SDL2 are
# visible to every target that includes them.
include_directories(BEFORE "${PS4_SYSROOT}/include")

# pthread is in libkernel on PS4. Keep STK's find_library from resolving the
# host's libpthread, which is both the wrong ABI and the wrong architecture.
# OpenOrbis links with ld.lld directly, so a compiler-driver -pthread flag is
# invalid. libc++/libkernel provide pthread symbols without a separate archive.
set(PTHREAD_LIBRARY "" CACHE STRING "" FORCE)

# --- Port libraries ---------------------------------------------------------
# ps4-kit's contract: a port's own libraries are installed into the sysroot
# ahead of PacBrew's. Each missing library gets an actionable message instead
# of STK's generic find_package failure.

function(ps4_find_sysroot_lib out_var)
    find_library(${out_var} NAMES ${ARGN}
        PATHS "${PS4_SYSROOT}/lib" NO_DEFAULT_PATH)
endfunction()

ps4_find_sysroot_lib(PS4_SDL2_LIB SDL2 libSDL2)
find_path(PS4_SDL2_INC NAMES SDL.h
    PATHS "${PS4_SYSROOT}/include/SDL2" "${PS4_SYSROOT}/include"
    NO_DEFAULT_PATH)
if(PS4_SDL2_LIB AND PS4_SDL2_INC)
    # STK looks these up with find_library/find_path; pre-seed the cache.
    set(SDL2_LIBRARY    "${PS4_SDL2_LIB}" CACHE FILEPATH "" FORCE)
    set(SDL2_INCLUDEDIR "${PS4_SDL2_INC}" CACHE PATH     "" FORCE)
else()
    message(FATAL_ERROR
        "SDL2 for PS4 not found in ${PS4_SYSROOT}.\n"
        "The port's SDL2 layer (video/events/pad/Vulkan + SDL audio for "
        "MojoAL) must implement the symbols in ps4/SDL_API_SURFACE.md and be "
        "installed as ${PS4_SYSROOT}/lib/libSDL2.a with headers in "
        "${PS4_SYSROOT}/include/SDL2.")
endif()

if(NO_SHADERC)
    message(FATAL_ERROR
        "NO_SHADERC=ON is not a valid PS4 configuration: it compiles out the "
        "only usable renderer (Vulkan). Build shaderc for the sysroot instead.")
endif()
ps4_find_sysroot_lib(PS4_SHADERC_LIB shaderc_combined libshaderc_combined shaderc libshaderc)
find_path(PS4_SHADERC_INC NAMES shaderc/shaderc.hpp
    PATHS "${PS4_SYSROOT}/include" NO_DEFAULT_PATH)
if(PS4_SHADERC_LIB AND PS4_SHADERC_INC)
    set(SHADERC_LIBRARY    "${PS4_SHADERC_LIB}" CACHE FILEPATH "" FORCE)
    set(SHADERC_INCLUDEDIR "${PS4_SHADERC_INC}" CACHE PATH     "" FORCE)
else()
    message(FATAL_ERROR
        "shaderc for PS4 not found in ${PS4_SYSROOT}.\n"
        "STK's Vulkan renderer compiles its GLSL shaders to SPIR-V at "
        "runtime (lib/graphics_engine/src/ge_vulkan_shader_manager.cpp); "
        "with -DNO_SHADERC=ON loadShader() returns VK_NULL_HANDLE and Vulkan "
        "cannot be used on PS4. Cross-build shaderc with the ps4-kit "
        "toolchain and install it into the sysroot.")
endif()

# STK needs one crypto provider for networking. The kit's seed sysroot
# already ships OpenSSL, so use it and avoid another cross-build; fall back
# to mbedTLS only for a lean sysroot.
ps4_find_sysroot_lib(PS4_SSL_LIB ssl libssl)
ps4_find_sysroot_lib(PS4_CRYPTO_LIB crypto libcrypto)
find_path(PS4_OPENSSL_INC NAMES openssl/ssl.h
    PATHS "${PS4_SYSROOT}/include" NO_DEFAULT_PATH)
if(PS4_SSL_LIB AND PS4_CRYPTO_LIB AND PS4_OPENSSL_INC)
    set(USE_CRYPTO_OPENSSL ON CACHE BOOL "" FORCE)
    set(OPENSSL_INCLUDE_DIR    "${PS4_OPENSSL_INC}" CACHE PATH     "" FORCE)
    set(OPENSSL_SSL_LIBRARY    "${PS4_SSL_LIB}"     CACHE FILEPATH "" FORCE)
    set(OPENSSL_CRYPTO_LIBRARY "${PS4_CRYPTO_LIB}"  CACHE FILEPATH "" FORCE)
else()
    ps4_find_sysroot_lib(PS4_MBEDCRYPTO_LIB mbedcrypto libmbedcrypto)
    find_path(PS4_MBEDTLS_INC NAMES mbedtls/version.h
        PATHS "${PS4_SYSROOT}/include" NO_DEFAULT_PATH)
    if(PS4_MBEDCRYPTO_LIB AND PS4_MBEDTLS_INC)
        set(USE_CRYPTO_OPENSSL OFF CACHE BOOL "" FORCE)
        set(MBEDCRYPTO_LIBRARY   "${PS4_MBEDCRYPTO_LIB}" CACHE FILEPATH "" FORCE)
        set(MBEDTLS_INCLUDE_DIRS "${PS4_MBEDTLS_INC}"    CACHE PATH     "" FORCE)
    else()
        message(FATAL_ERROR
            "No crypto provider for PS4 in ${PS4_SYSROOT}.\n"
            "Install OpenSSL (libssl.a, libcrypto.a + headers; present in "
            "the kit's seed sysroot) or mbedTLS (libmbedcrypto.a + headers).")
    endif()
endif()

# Keep pkg-config inside the PS4 sysroot. STK's FindOggVorbis uses
# pkg_check_modules on Unix; without this it can resolve host libraries.
set(ENV{PKG_CONFIG_LIBDIR}
    "${PS4_SYSROOT}/lib/pkgconfig:${PS4_SYSROOT}/share/pkgconfig")

# Image/text/audio libraries. STK's find_package calls run after this file;
# seed both the modern and compatibility cache names so they cannot pick a
# host library from /usr.
ps4_find_sysroot_lib(PS4_JPEG_LIB jpeg libjpeg)
find_path(PS4_JPEG_INC NAMES jpeglib.h
    PATHS "${PS4_SYSROOT}/include" NO_DEFAULT_PATH)
if(PS4_JPEG_LIB AND PS4_JPEG_INC)
    set(JPEG_LIBRARY    "${PS4_JPEG_LIB}" CACHE FILEPATH "" FORCE)
    set(JPEG_LIBRARIES  "${PS4_JPEG_LIB}" CACHE FILEPATH "" FORCE)
    set(JPEG_INCLUDE_DIR "${PS4_JPEG_INC}" CACHE PATH "" FORCE)
    set(JPEG_INCLUDE_DIRS "${PS4_JPEG_INC}" CACHE PATH "" FORCE)
endif()

ps4_find_sysroot_lib(PS4_HARFBUZZ_LIB harfbuzz libharfbuzz)
find_path(PS4_HARFBUZZ_INC NAMES harfbuzz/hb.h
    PATHS "${PS4_SYSROOT}/include" NO_DEFAULT_PATH)
if(PS4_HARFBUZZ_LIB AND PS4_HARFBUZZ_INC)
    set(HARFBUZZ_LIBRARY    "${PS4_HARFBUZZ_LIB}" CACHE FILEPATH "" FORCE)
    set(HARFBUZZ_INCLUDEDIR "${PS4_HARFBUZZ_INC}" CACHE PATH "" FORCE)
endif()

ps4_find_sysroot_lib(PS4_SAMPLERATE_LIB samplerate libsamplerate)
find_path(PS4_SAMPLERATE_INC NAMES samplerate.h
    PATHS "${PS4_SYSROOT}/include" NO_DEFAULT_PATH)
if(PS4_SAMPLERATE_LIB AND PS4_SAMPLERATE_INC)
    set(LIBSAMPLERATE_LIBRARY    "${PS4_SAMPLERATE_LIB}" CACHE FILEPATH "" FORCE)
    set(LIBSAMPLERATE_INCLUDEDIR "${PS4_SAMPLERATE_INC}" CACHE PATH "" FORCE)
endif()

ps4_find_sysroot_lib(PS4_FREETYPE_LIB freetype libfreetype)
find_path(PS4_FREETYPE_INC NAMES ft2build.h
    PATHS "${PS4_SYSROOT}/include/freetype2" NO_DEFAULT_PATH)
if(PS4_FREETYPE_LIB AND PS4_FREETYPE_INC)
    set(FREETYPE_LIBRARY         "${PS4_FREETYPE_LIB}" CACHE FILEPATH "" FORCE)
    set(FREETYPE_LIBRARY_RELEASE "${PS4_FREETYPE_LIB}" CACHE FILEPATH "" FORCE)
    set(FREETYPE_INCLUDE_DIRS    "${PS4_FREETYPE_INC}" CACHE PATH "" FORCE)
    set(FREETYPE_INCLUDE_DIR_ft2build   "${PS4_FREETYPE_INC}" CACHE PATH "" FORCE)
    set(FREETYPE_INCLUDE_DIR_freetype2  "${PS4_FREETYPE_INC}" CACHE PATH "" FORCE)
endif()

ps4_find_sysroot_lib(PS4_PNG_LIB png libpng)
find_path(PS4_PNG_INC NAMES png.h PATHS "${PS4_SYSROOT}/include" NO_DEFAULT_PATH)
if(PS4_PNG_LIB AND PS4_PNG_INC)
    set(PNG_LIBRARY         "${PS4_PNG_LIB}" CACHE FILEPATH "" FORCE)
    set(PNG_LIBRARY_RELEASE "${PS4_PNG_LIB}" CACHE FILEPATH "" FORCE)
    set(PNG_PNG_INCLUDE_DIR "${PS4_PNG_INC}" CACHE PATH "" FORCE)
    set(PNG_INCLUDE_DIR     "${PS4_PNG_INC}" CACHE PATH "" FORCE)
    set(PNG_INCLUDE_DIRS    "${PS4_PNG_INC}" CACHE PATH "" FORCE)
endif()

ps4_find_sysroot_lib(PS4_CURL_LIB curl libcurl)
find_path(PS4_CURL_INC NAMES curl/curl.h PATHS "${PS4_SYSROOT}/include" NO_DEFAULT_PATH)
if(PS4_CURL_LIB AND PS4_CURL_INC)
    set(CURL_LIBRARY         "${PS4_CURL_LIB}" CACHE FILEPATH "" FORCE)
    set(CURL_LIBRARY_RELEASE "${PS4_CURL_LIB}" CACHE FILEPATH "" FORCE)
    set(CURL_INCLUDE_DIR     "${PS4_CURL_INC}" CACHE PATH "" FORCE)
    set(CURL_INCLUDE_DIRS    "${PS4_CURL_INC}" CACHE PATH "" FORCE)
endif()

set(ZLIB_LIBRARY    "${PS4_SYSROOT}/lib/libz.a" CACHE FILEPATH "" FORCE)
set(ZLIB_INCLUDE_DIR "${PS4_SYSROOT}/include"  CACHE PATH     "" FORCE)
