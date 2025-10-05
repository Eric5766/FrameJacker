# FrameJacker
A lightweight graphics API hooking library for Windows with CMake integration, created to take all of the pain out of hooking graphics APIs.

Provides a simple framework to add graphics API hooks to your projects to call your own code with minimal effort required.

All required headers/dependencies are included either in the project or via CMAKE ([ByteWeaver](https://github.com/0xKate/ByteWeaver/) and [Detours](https://github.com/microsoft/Detours))

## Features
- **Multi-API Support**: DirectX 9/10/11/12, OpenGL, and Vulkan (Planned)
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
| Vulkan | ✗ | - | - | 

## Tested & Working
| API | x86 (32-bit) | x64 (64-bit) |
|-----|--------------|--------------|
| DirectX 9 | ✓ | ? (Rare) |
| DirectX 10 | ✓ | ✓ |
| DirectX 11 | ? (Rare) | ✓ |
| DirectX 12 | ✗ (x64 only) | ✓ |
| OpenGL | ✓ | ✓ |
| Vulkan | ✗ | ✗ |

*Vulkan is currently not working as the process for hooking it is far more complicated than DX.*

*I'm not aware of any DX9 64 bit games nor any 32 bit DX11 games, but the architecture shouldn't matter anyway*

✓ = Tested and working  
? = Untested  
✗ = Known issues


## CMake Integration
```cmake
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
        
        // Initialize hook - Possible APIs:
        //FrameJacker::API::Auto - This detects which API is in use by checking loaded DLLs. Warning -
        //Some games load in multiple Graphics APIs even if not using them, so generally it'll always be better to specify the one you are using.

        //FrameJacker::API::D3D9
        //FrameJacker::API::D3D10
        //FrameJacker::API::D3D11
        //FrameJacker::API::D3D12
        //FrameJacker::API::OpenGL
        //FrameJacker::API::Vulkan (Incomplete/non-working)
        FrameJacker::Hook::Initialize(FrameJacker::API::D3D12);
    }
    return TRUE;
}
```
