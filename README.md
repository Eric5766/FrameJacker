# FrameJacker
A lightweight graphics API hooking library for Windows with CMake integration, created to take all of the pain out of hooking graphics APIs.

Provides a simple framework to add graphics API hooks to your projects to call your own code with minimal effort required.

All required headers/dependencies are included either in the project or via CMAKE ([ByteWeaver](https://github.com/0xKate/ByteWeaver/) and [Detours](https://github.com/microsoft/Detours))

## Features
- **Multi-API Support**: DirectX 9/10/11/12, OpenGL, and Vulkan
- **Simple Callback System**: Hook into frame presentation and resize events
- **CMake Integration**: Easy to integrate via FetchContent

## Supported Callbacks by API

| API | OnPresent | OnResize | OnDeviceCreated | OnRender |
|-----|-----------|----------|-----------------|----------|
| DirectX 9 | ✓ | ✓ | - | ✓ |
| DirectX 10 | ✓ | ✓ | - | ✓ |
| DirectX 11 | ✓ | ✓ | - | ✓ |
| DirectX 12 | ✓ | ✓ | ✓ | ✓ |
| OpenGL | ✓ | - | - | ✓ |
| Vulkan | ✓ | ✓ | - | ✓ |

**Notes:**
- `OnPresent`: Called every frame when the application presents/swaps buffers
- `OnResize`: Called when swap chain buffers are resized (not supported in OpenGL)
- `OnDeviceCreated`: Called when the graphics device/queue becomes available (DX12 only due to architectural differences)
- `OnRender`: Provides unified `RenderContext` with API-specific device/context pointers for custom rendering (e.g., ImGui integration)

## Tested & Working
| API | x86 (32-bit) | x64 (64-bit) |
|-----|--------------|--------------|
| DirectX 9 | ✓ | ✓ ) |
| DirectX 10 | ✓ | ✓ |
| DirectX 11 | ✓  | ✓ |
| DirectX 12 | ✗ (DX12 = x64 only) | ✓ |
| OpenGL | ✓ | ✓ |
| Vulkan | ✓ | ✓ |


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
    GIT_TAG 0.1.1
)
FetchContent_MakeAvailable(FrameJacker)

# Link to your target
target_link_libraries(YourProject PRIVATE FrameJacker)
```

## Simple Usage Example

```cpp
#include <FrameJacker.h>
#include <Windows.h>
#include <cstdio>

void OnFrame() {
    static int count = 0;
    printf("Frame %d\n", count++);
    fflush(stdout);
}

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD fdwReason, LPVOID) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hInstance);
        AllocConsole();
        freopen("CONOUT$", "w", stdout);
        
        FrameJacker::Callbacks callbacks;
        callbacks.OnPresent = OnFrame;
        FrameJacker::Hook::SetCallbacks(callbacks);

        // Initialize hook - Possible APIs:
        //FrameJacker::API::Auto - This detects which API is in use by checking loaded DLLs. Warning -
        //Some games load in multiple Graphics APIs even if not using them, so generally it'll always be better to specify the one you are using.

        //FrameJacker::API::D3D9
        //FrameJacker::API::D3D10
        //FrameJacker::API::D3D11
        //FrameJacker::API::D3D12
        //FrameJacker::API::OpenGL
        //FrameJacker::API::Vulkan
        FrameJacker::Hook::Initialize(FrameJacker::API::D3D9);
    }
    return TRUE;
}
```

## Extended Usage Example

```cpp
#include <FrameJacker.h>
#include <Windows.h>
#include <iostream>

void OnFrame() {
    static int frameCount = 0;
    printf("Frame: %d\n", frameCount++);
}

void OnWindowResize() {
    printf("Window resized!\n");
}

void OnDeviceReady(void* device) {
    printf("Graphics device created at: %p\n", device);
}

void OnRender(const FrameJacker::RenderContext& ctx) {
    // This callback provides everything needed for ImGui or custom rendering

    switch (ctx.api) {
    case FrameJacker::API::D3D9: {
        // For DX9: ctx.device is IDirect3DDevice9*
        // auto* device = static_cast<IDirect3DDevice9*>(ctx.device);
        // ImGui_ImplDX9_NewFrame();
        // ImGui::NewFrame();
        // /* Your ImGui rendering code */
        // ImGui::Render();
        // ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        break;
    }

    case FrameJacker::API::D3D10: {
        // For DX10: ctx.device is ID3D10Device*, ctx.swapChain is IDXGISwapChain*
        // auto* device = static_cast<ID3D10Device*>(ctx.device);
        break;
    }

    case FrameJacker::API::D3D11: {
        // For DX11: ctx.device is ID3D11Device*, ctx.commandBuffer is ID3D11DeviceContext*
        // auto* device = static_cast<ID3D11Device*>(ctx.device);
        // auto* context = static_cast<ID3D11DeviceContext*>(ctx.commandBuffer);
        // ImGui_ImplDX11_NewFrame();
        // ImGui::NewFrame();
        // /* Your ImGui rendering code */
        // ImGui::Render();
        // ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        break;
    }

    case FrameJacker::API::D3D12: {
        // For DX12: ctx.extra is ID3D12CommandQueue*
        // auto* commandQueue = static_cast<ID3D12CommandQueue*>(ctx.extra);
        // auto* swapChain = static_cast<IDXGISwapChain3*>(ctx.swapChain);
        // /* DX12 requires more setup - create command lists, etc. */
        break;
    }

    case FrameJacker::API::OpenGL: {
        // For OpenGL: ctx.extra is HDC (device context handle)
        // HDC hdc = static_cast<HDC>(ctx.extra);
        // ImGui_ImplOpenGL3_NewFrame();
        // ImGui::NewFrame();
        // /* Your ImGui rendering code */
        // ImGui::Render();
        // ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        break;
    }

    case FrameJacker::API::Vulkan: {
        // For Vulkan: ctx.device is VkDevice, ctx.extra is VkQueue
        // VkDevice device = static_cast<VkDevice>(ctx.device);
        // VkQueue queue = static_cast<VkQueue>(ctx.extra);
        // VkSwapchainKHR swapchain = static_cast<VkSwapchainKHR>(ctx.swapChain);
        // uint32_t imageIndex = ctx.imageIndex;
        // /* Vulkan requires creating command buffers and render passes */
        break;
    }
    }
}

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD fdwReason, LPVOID) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hInstance);

        // Optional: Enable debug logging
        FrameJacker::SetDebugLogging(true);

        // Optional: Set up console for debugging
        AllocConsole();
        freopen("CONOUT$", "w", stdout);

        // Set up callbacks
        FrameJacker::Callbacks callbacks;
        callbacks.OnPresent = OnFrame;           // Called every frame
        callbacks.OnResize = OnWindowResize;     // Called on window resize
        callbacks.OnDeviceCreated = OnDeviceReady; // Called when device is created
        callbacks.OnRender = OnRender;           // Called before present with render context

        FrameJacker::Hook::SetCallbacks(callbacks);

        // Initialize with auto-detection or specify an API
        if (FrameJacker::Hook::Initialize(FrameJacker::API::D3D9)) {
            printf("FrameJacker initialized successfully!\n");
            printf("Active API: %d\n", static_cast<int>(FrameJacker::Hook::GetActiveAPI()));
        }
        else {
            printf("FrameJacker initialization failed!\n");
        }
    }
    else if (fdwReason == DLL_PROCESS_DETACH) {
        FrameJacker::Hook::Shutdown();
    }

    return TRUE;
}
```

