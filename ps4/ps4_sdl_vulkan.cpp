// SuperTuxKart PS4: SDL's Vulkan entry points on ps4-kit's WSI.
//
// SDL2 is built with the null video driver: it supplies a window and event
// queue, but no Vulkan surface.  ps4-kit's ps4_vk_wsi.cpp adds
// VK_EXT_headless_surface and VK_KHR_swapchain on VideoOut; these are the four
// SDL calls STK's Graphics Engine uses.  The linker wraps keep SDL's own
// (VULKAN=OFF) stubs out of the call path.

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vulkan/vulkan.h>

#include "ps4_vk_wsi.h"
#include "ps4_kit_diag.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

extern "C" void ps4_input_start(void);

namespace
{
const char* const kInstanceExtensions[] = {
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME,
};
constexpr unsigned kInstanceExtensionCount =
    sizeof(kInstanceExtensions) / sizeof(kInstanceExtensions[0]);

PFN_vkGetInstanceProcAddr instance_proc_addr()
{
    return reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        ps4kit::vk_get_instance_proc_addr());
}

// RADV's PS4 flight recorder / fault capture is opt-in through the driver's
// environment.  A marker file lets a console run be switched into trace mode
// over FTP, with no rebuild.  Must run before the instance is created.
void configure_radv_trace()
{
    static bool done = false;
    if (done)
        return;
    done = true;

    // The WSI step trace is what names the last call before a fault that leaves no crash.log (the
    // system ending the app). Its default budget covers only the first seconds, which is where the
    // old traces ended; a race load needs thousands of lines. 20000 lines (~1 MiB) is a few minutes
    // of frames and is kept in boot-trace.log, per boot, so the failure is never cut off again.
    ps4kit::set_step_trace(20000);

    std::FILE* marker = std::fopen("/data/stk/vk-trace", "r");
    if (!marker)
        return;
    std::fclose(marker);

    setenv("RADV_PS4_TRACE", "1", 1);
    ps4_boot_trace("port: RADV_PS4_TRACE=1 (/data/stk/vk-trace)");
}
} // namespace

extern "C" void* __wrap_SDL_Vulkan_GetVkGetInstanceProcAddr(void)
{
    configure_radv_trace();
    ps4_boot_trace("port: SDL_Vulkan_GetVkGetInstanceProcAddr (Vulkan handoff)");
    return ps4kit::vk_get_instance_proc_addr();
}

extern "C" SDL_bool __wrap_SDL_Vulkan_GetInstanceExtensions(
    SDL_Window*, unsigned int* count, const char** names)
{
    ps4_input_start();
    if (!count)
        return SDL_FALSE;

    if (!names)
    {
        *count = kInstanceExtensionCount;
        return SDL_TRUE;
    }

    const unsigned int written =
        std::min(*count, kInstanceExtensionCount);
    for (unsigned int i = 0; i < written; ++i)
        names[i] = kInstanceExtensions[i];
    *count = written;

    ps4_boot_tracef("port: SDL_Vulkan_GetInstanceExtensions -> %u", written);
    return written == kInstanceExtensionCount ? SDL_TRUE : SDL_FALSE;
}

extern "C" SDL_bool __wrap_SDL_Vulkan_CreateSurface(
    SDL_Window*, VkInstance instance, VkSurfaceKHR* surface)
{
    ps4_input_start();
    if (!surface)
        return SDL_FALSE;

    PFN_vkGetInstanceProcAddr get_proc = instance_proc_addr();
    if (!get_proc)
        return SDL_FALSE;

    auto create = reinterpret_cast<PFN_vkCreateHeadlessSurfaceEXT>(
        get_proc(instance, "vkCreateHeadlessSurfaceEXT"));
    if (!create)
    {
        ps4_boot_trace("port: SDL_Vulkan_CreateSurface: vkCreateHeadlessSurfaceEXT missing");
        return SDL_FALSE;
    }

    VkHeadlessSurfaceCreateInfoEXT info = {};
    info.sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT;
    info.pNext = nullptr;
    info.flags = 0;

    const VkResult result = create(instance, &info, nullptr, surface);
    ps4_boot_tracef("port: SDL_Vulkan_CreateSurface -> VkResult %d", static_cast<int>(result));
    return result == VK_SUCCESS
        ? SDL_TRUE
        : SDL_FALSE;
}

extern "C" void __wrap_SDL_Vulkan_GetDrawableSize(
    SDL_Window*, int* width, int* height)
{
    if (width)
        *width = static_cast<int>(ps4kit::kScreenWidth);
    if (height)
        *height = static_cast<int>(ps4kit::kScreenHeight);
    static bool logged;
    if (!logged)
    {
        logged = true;
        ps4_boot_trace("port: SDL_Vulkan_GetDrawableSize");
    }
}
