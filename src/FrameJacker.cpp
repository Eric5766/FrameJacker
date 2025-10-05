#include "FrameJacker.h"
#include <Windows.h>

namespace FrameJacker {
    std::unique_ptr<IGraphicsHook> Hook::s_ActiveHook = nullptr;
    Callbacks Hook::s_Callbacks = {};
    bool FrameJacker::g_EnableDebugLogging = false;
    FrameJacker::LogFunction FrameJacker::g_CustomLogHandler = nullptr;

    void FrameJacker::SetDebugLogging(bool enabled) {
        g_EnableDebugLogging = enabled;
    }

    void FrameJacker::SetLogHandler(LogFunction handler) {
        g_CustomLogHandler = handler;
    }


    const char* APIToString(API api) {
        switch (api) {
        case API::D3D9: return "D3D9";
        case API::D3D10: return "D3D10";
        case API::D3D11: return "D3D11";
        case API::D3D12: return "D3D12";
        case API::OpenGL: return "OpenGL";
        case API::Vulkan: return "Vulkan";
        default: return "Unknown";
        }
    }

    bool Hook::Initialize(API api) {
        if (s_ActiveHook)
            return false;

        if (api == API::Auto) {
            if (GetModuleHandleW(L"d3d12.dll")) api = API::D3D12;
            else if (GetModuleHandleW(L"d3d11.dll")) api = API::D3D11;
            else if (GetModuleHandleW(L"d3d10.dll")) api = API::D3D10;
            else if (GetModuleHandleW(L"d3d9.dll")) api = API::D3D9;
            else if (GetModuleHandleW(L"opengl32.dll")) api = API::OpenGL;
            else if (GetModuleHandleW(L"vulkan-1.dll")) api = API::Vulkan;
            else {
                DEBUG_LOG("No supported graphics API detected");
                return false;
            }
        }

        DEBUG_LOG("Detected API: %s", APIToString(api));

        switch (api) {
        case API::D3D9:
#if FRAMEJACKER_INCLUDE_D3D9
            s_ActiveHook = std::make_unique<DX9Hook>();
            break;
#endif
        case API::D3D10:
#if FRAMEJACKER_INCLUDE_D3D10
            s_ActiveHook = std::make_unique<DX10Hook>();
            break;
#endif
        case API::D3D11:
#if FRAMEJACKER_INCLUDE_D3D11
            s_ActiveHook = std::make_unique<DX11Hook>();
            break;
#endif
        case API::D3D12:
#if FRAMEJACKER_INCLUDE_D3D12
            s_ActiveHook = std::make_unique<DX12Hook>();
            break;
#endif
        case API::OpenGL:
#if FRAMEJACKER_INCLUDE_OPENGL
            s_ActiveHook = std::make_unique<OpenGLHook>();
            break;
#endif
        case API::Vulkan:
#if FRAMEJACKER_INCLUDE_VULKAN
            s_ActiveHook = std::make_unique<VulkanHook>();
            break;
#endif
        default:
            DEBUG_LOG("API %s not yet implemented", APIToString(api));
            return false;
        }

        return s_ActiveHook->Install();
    }

    void Hook::Shutdown() {
        if (s_ActiveHook) {
            s_ActiveHook->Uninstall();
            s_ActiveHook.reset();
        }
    }

    void Hook::SetCallbacks(const Callbacks& callbacks) {
        s_Callbacks = callbacks;
    }

    API Hook::GetActiveAPI() {
        return s_ActiveHook ? s_ActiveHook->GetAPI() : API::Auto;
    }



}