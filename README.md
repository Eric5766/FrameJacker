# FrameJacker
A lightweight graphics API hooking library for Windows with CMake integration, created to take all of the pain out of hooking graphics APIs.

Provides a simple framework to add graphics API hooks to your projects to call your own code with minimal effort required.

All required headers/dependencies are included either in the project or via CMAKE ([ByteWeaver](https://github.com/0xKate/ByteWeaver/) and [Detours](https://github.com/microsoft/Detours))

## Features
- **Multi-API Support**: DirectX 9/10/11/12, OpenGL, and Vulkan
- **Modular**: Include only the APIs you need via CMAKE 
- **Automatic Detection**: Auto-detects which graphics API is being used
- **Simple Callback System**: Hook into frame presentation and resize events
- **CMake Integration**: Easy to integrate via FetchContent

## Supported APIs & Callbacks
| API | OnPresent | OnResize | OnDeviceCreated |
|-----|-----------|----------|-----------------|
| DirectX 9 | ✓ | ✓ | - |
| DirectX 10 | ✓ | - | - |
| DirectX 11 | ✓ | - | - |
| DirectX 12 | ✓ | - | ✓ |
| OpenGL | ✓ | - | - |
| Vulkan | ✓ | - | - |

## Tested & Working
| API | x86 (32-bit) | x64 (64-bit) |
|-----|--------------|--------------|
| DirectX 9 | ✓ | ? |
| DirectX 10 | ? | ? |
| DirectX 11 | ? | ? |
| DirectX 12 | ? | ✓ |
| OpenGL | ? | ? |
| Vulkan | ? | ? |

✓ = Tested and working  
? = Untested  
✗ = Known issues

*Note: This library is in its very early stages and is largely untested*

## CMake Integration
```cmake
# Configure which APIs to include - This will build the library with only the selected APIs.
set(FRAMEJACKER_D3D9 ON CACHE BOOL "" FORCE)
set(FRAMEJACKER_D3D10 OFF CACHE BOOL "" FORCE)
set(FRAMEJACKER_D3D11 OFF CACHE BOOL "" FORCE)
set(FRAMEJACKER_D3D12 OFF CACHE BOOL "" FORCE)
set(FRAMEJACKER_VULKAN OFF CACHE BOOL "" FORCE)
set(FRAMEJACKER_OPENGL OFF CACHE BOOL "" FORCE)

# Fetch FrameJacker
include(FetchContent)
FetchContent_Declare(
    FrameJacker
    GIT_REPOSITORY https://github.com/Eric5766/FrameJacker.git
    GIT_TAG 0.1.0
)
FetchContent_MakeAvailable(FrameJacker)

# Link to your target
target_link_libraries(YourProject PRIVATE FrameJacker)
```

## Simple Usage Example

```cpp
#include <FrameJacker.h>
#include <Windows.h>
#include <iostream>

void OnFrame() {
    // Your per-frame logic here
    // This is called every time the application presents a frame
}

void OnWindowResize() {
    // Handle window/swapchain resize events
}

void OnDeviceReady(void* device) {
    // For DX12: ID3D12CommandQueue*
    // Access the graphics device when it's created
}

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD fdwReason, LPVOID) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hInstance);
        
        // Optional: Enable debug logging
        FrameJacker::SetDebugLogging(true);

        // Optional: Set up console
        AllocConsole();
        freopen("CONOUT$", "w", stdout);

        // Set up callbacks - Check which are appropriate for which API above
        FrameJacker::Callbacks callbacks;
        callbacks.OnPresent = OnFrame;
        callbacks.OnResize = OnWindowResize;
        callbacks.OnDeviceCreated = OnDeviceReady;
        
        FrameJacker::Hook::SetCallbacks(callbacks);
        
        // Initialize with auto-detection or specify an API
        FrameJacker::Hook::Initialize(FrameJacker::API::Auto);
    }
    return TRUE;
}```
