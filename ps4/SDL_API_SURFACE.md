# SDL2 API surface required by STK

Generated from the checked-out source by `ps4/tools/sdl_surface.sh`.
A PS4 SDL2 layer must define every symbol listed here.

## src

| symbol | refs |
|---|---:|
| `SDL_IsAndroidTV` | 9 |
| `SDL_GetError` | 9 |
| `SDL_GameControllerClose` | 7 |
| `SDL_AndroidGetJNIEnv` | 7 |
| `SDL_VERSION_ATLEAST` | 6 |
| `SDL_JoystickClose` | 6 |
| `SDL_AndroidGetActivity` | 6 |
| `SDL_RWclose` | 4 |
| `SDL_AndroidGetInternalStoragePath` | 4 |
| `SDL_StopTextInput` | 3 |
| `SDL_JoystickName` | 3 |
| `SDL_GetAndroidSDKVersion` | 3 |
| `SDL_free` | 3 |
| `SDL_RWFromFile` | 2 |
| `SDL_JoystickOpen` | 2 |
| `SDL_JoystickInstanceID` | 2 |
| `SDL_IsTextInputActive` | 2 |
| `SDL_InitSubSystem` | 2 |
| `SDL_GetDisplayOrientation` | 2 |
| `SDL_GameControllerName` | 2 |
| `SDL_arraysize` | 2 |
| `SDL_AndroidGetExternalStoragePath` | 2 |
| `SDL_VERSION` | 1 |
| `SDL_StartTextInput` | 1 |
| `SDL_SetTextInputRect` | 1 |
| `SDL_SetHint` | 1 |
| `SDL_setenv` | 1 |
| `SDL_RWread` | 1 |
| `SDL_Quit` | 1 |
| `SDL_OpenURL` | 1 |
| `SDL_NumJoysticks` | 1 |
| `SDL_main` | 1 |
| `SDL_JoystickNumHats` | 1 |
| `SDL_JoystickNumButtons` | 1 |
| `SDL_JoystickNumAxes` | 1 |
| `SDL_JoystickGetAxis` | 1 |
| `SDL_JoystickCurrentPowerLevel` | 1 |
| `SDL_IsGameController` | 1 |
| `SDL_IsChromebook` | 1 |
| `SDL_HapticSetAutocenter` | 1 |
| `SDL_HapticRumblePlay` | 1 |
| `SDL_HapticRumbleInit` | 1 |
| `SDL_HapticOpenFromJoystick` | 1 |
| `SDL_HapticClose` | 1 |
| `SDL_GetWMInfo` | 1 |
| `SDL_GetSystemRAM` | 1 |
| `SDL_GetPreferredLocales` | 1 |
| `SDL_GetNumVideoDisplays` | 1 |
| `SDL_GetCurrentDisplayMode` | 1 |
| `SDL_GameControllerRumble` | 1 |
| `SDL_GameControllerOpen` | 1 |
| `SDL_GameControllerMapping` | 1 |
| `SDL_GameControllerGetJoystick` | 1 |

## lib/irrlicht/source

| symbol | refs |
|---|---:|
| `SDL_GL_SetAttribute` | 30 |
| `SDL_VERSION_ATLEAST` | 15 |
| `SDL_SetHint` | 10 |
| `SDL_GL_DeleteContext` | 6 |
| `SDL_DestroyWindow` | 6 |
| `SDL_CreateWindow` | 6 |
| `SDL_AndroidGetJNIEnv` | 6 |
| `SDL_AndroidGetActivity` | 6 |
| `SDL_GL_CreateContext` | 5 |
| `SDL_GetError` | 4 |
| `SDL_BITSPERPIXEL` | 4 |
| `SDL_Vulkan_UnloadLibrary` | 3 |
| `SDL_GetDisplayOrientation` | 3 |
| `SDL_ShowCursor` | 2 |
| `SDL_SensorOpen` | 2 |
| `SDL_SensorGetInstanceID` | 2 |
| `SDL_SensorGetDeviceType` | 2 |
| `SDL_SensorFromInstanceID` | 2 |
| `SDL_SensorClose` | 2 |
| `SDL_ISPIXELFORMAT_ALPHA` | 2 |
| `SDL_InitSubSystem` | 2 |
| `SDL_GL_SwapWindow` | 2 |
| `SDL_GL_SetSwapInterval` | 2 |
| `SDL_GL_GetProcAddress` | 2 |
| `SDL_GL_GetDrawableSize` | 2 |
| `SDL_GetWindowWMInfo` | 2 |
| `SDL_free` | 2 |
| `SDL_Delay` | 2 |
| `SDL_WarpMouseGlobal` | 1 |
| `SDL_Vulkan_LoadLibrary` | 1 |
| `SDL_VERSION` | 1 |
| `SDL_UIKitRunApp` | 1 |
| `SDL_Swap32` | 1 |
| `SDL_Swap16` | 1 |
| `SDL_SetWindowTitle` | 1 |
| `SDL_SetWindowSize` | 1 |
| `SDL_SetWindowResizable` | 1 |
| `SDL_SetWindowPosition` | 1 |
| `SDL_SetWindowMinimumSize` | 1 |
| `SDL_SetWindowFullscreen` | 1 |
| `SDL_SetGamma` | 1 |
| `SDL_SetEventFilter` | 1 |
| `SDL_SetClipboardText` | 1 |
| `SDL_RaiseWindow` | 1 |
| `SDL_QuitSubSystem` | 1 |
| `SDL_PollEvent` | 1 |
| `SDL_NumSensors` | 1 |
| `SDL_HasScreenKeyboardSupport` | 1 |
| `SDL_GetWindowSize` | 1 |
| `SDL_GetWindowPosition` | 1 |
| `SDL_GetWindowPixelFormat` | 1 |
| `SDL_GetWindowDisplayIndex` | 1 |
| `SDL_GetVersion` | 1 |
| `SDL_GetScreenKeyboardHeight` | 1 |
| `SDL_GetNumVideoDisplays` | 1 |
| `SDL_GetNumTouchDevices` | 1 |
| `SDL_GetNumDisplayModes` | 1 |
| `SDL_GetMovedHeightByScreenKeyboard` | 1 |
| `SDL_GetGamma` | 1 |
| `SDL_GetDisplayMode` | 1 |
| `SDL_GetDisplayDPI` | 1 |
| `SDL_GetDesktopDisplayMode` | 1 |
| `SDL_GetClipboardText` | 1 |

## lib/graphics_engine/src

| symbol | refs |
|---|---:|
| `SDL_Vulkan_GetInstanceExtensions` | 2 |
| `SDL_Vulkan_CreateSurface` | 2 |
| `SDL_Vulkan_GetVkGetInstanceProcAddr` | 1 |
| `SDL_Vulkan_GetDrawableSize` | 1 |
| `SDL_HasSSE41` | 1 |
| `SDL_HasNEON` | 1 |
| `SDL_HasAVX2` | 1 |

## lib/mojoal

| symbol | refs |
|---|---:|
| `SDL_assert` | 55 |
| `SDL_memcpy` | 23 |
| `SDL_free` | 23 |
| `SDL_AtomicGet` | 23 |
| `SDL_AtomicSet` | 21 |
| `SDL_arraysize` | 13 |
| `SDL_QuitSubSystem` | 11 |
| `SDL_min` | 10 |
| `SDL_AtomicDecRef` | 10 |
| `SDL_AtomicCASPtr` | 9 |
| `SDL_UnlockMutex` | 8 |
| `SDL_LockMutex` | 8 |
| `SDL_calloc` | 8 |
| `SDL_AtomicGetPtr` | 8 |
| `SDL_AtomicAdd` | 8 |
| `SDL_UnlockAudioDevice` | 7 |
| `SDL_memset` | 7 |
| `SDL_strcmp` | 6 |
| `SDL_LockAudioDevice` | 6 |
| `SDL_max` | 5 |
| `SDL_zero` | 4 |
| `SDL_FreeAudioStream` | 4 |
| `SDL_AtomicSetPtr` | 4 |
| `SDL_sqrtf` | 3 |
| `SDL_realloc` | 3 |
| `SDL_PauseAudioDevice` | 3 |
| `SDL_DestroyMutex` | 3 |
| `SDL_cos` | 3 |
| `SDL_clamp` | 3 |
| `SDL_AtomicIncRef` | 3 |
| `SDL_acosf` | 3 |
| `SDL_zerop` | 2 |
| `SDL_strlcpy` | 2 |
| `SDL_strcasecmp` | 2 |
| `SDL_sinf` | 2 |
| `SDL_OpenAudioDevice` | 2 |
| `SDL_NewAudioStream` | 2 |
| `SDL_MemoryBarrierRelease` | 2 |
| `SDL_MemoryBarrierAcquire` | 2 |
| `SDL_malloc` | 2 |
| `SDL_log` | 2 |
| `SDL_InitSubSystem` | 2 |
| `SDL_HasNEON` | 2 |
| `SDL_GetAudioDeviceStatus` | 2 |
| `SDL_CreateMutex` | 2 |
| `SDL_cosf` | 2 |
| `SDL_CloseAudioDevice` | 2 |
| `SDL_strlen` | 1 |
| `SDL_strdup` | 1 |
| `SDL_sqrt` | 1 |
| `SDL_sin` | 1 |
| `SDL_powf` | 1 |
| `SDL_memmove` | 1 |
| `SDL_HasSSE` | 1 |
| `SDL_GetNumAudioDevices` | 1 |
| `SDL_GetAudioDeviceName` | 1 |
| `SDL_ConvertAudio` | 1 |
| `SDL_BuildAudioCVT` | 1 |
| `SDL_AudioStreamPut` | 1 |
| `SDL_AudioStreamGet` | 1 |
| `SDL_AudioStreamClear` | 1 |
| `SDL_AudioStreamAvailable` | 1 |
| `SDL_AUDIO_BITSIZE` | 1 |
| `SDL_AtomicCAS` | 1 |
| `SDL_atan2` | 1 |

