#include "FrameJacker.h"
#include <DetourMacros.hpp>
#include <MemoryManager.h>
#if FRAMEJACKER_INCLUDE_D3D11
#include <dxgi.h>
#include <d3d11.h>
#endif

using namespace ByteWeaver;

namespace FrameJacker {

    DECLARE_HOOK(DX11Present, HRESULT, __stdcall, __stdcall,
        IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);

    static uint150_t* g_MethodsTable = nullptr;

    void DX11Hook::InitializeMethodTable() {
        DEBUG_LOG("DX11 InitMethodTable starting...");

        WNDCLASSEXA windowClass = {};
        windowClass.cbSize = sizeof(WNDCLASSEXA);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = DefWindowProc;
        windowClass.hInstance = GetModuleHandle(NULL);
        windowClass.lpszClassName = "FrameJackerDX11";

        ::RegisterClassExA(&windowClass);
        HWND window = ::CreateWindowA(windowClass.lpszClassName, "FrameJacker DX11", WS_OVERLAPPEDWINDOW,
            0, 0, 100, 100, NULL, NULL, windowClass.hInstance, NULL);

        HMODULE libD3D11 = ::GetModuleHandleW(L"d3d11.dll");
        if (!libD3D11) {
            DEBUG_LOG("d3d11.dll not found");
            ::DestroyWindow(window);
            ::UnregisterClassA(windowClass.lpszClassName, windowClass.hInstance);
            return;
        }

        void* D3D11CreateDeviceAndSwapChain = ::GetProcAddress(libD3D11, "D3D11CreateDeviceAndSwapChain");
        if (!D3D11CreateDeviceAndSwapChain) {
            DEBUG_LOG("D3D11CreateDeviceAndSwapChain not found");
            ::DestroyWindow(window);
            ::UnregisterClassA(windowClass.lpszClassName, windowClass.hInstance);
            return;
        }

        D3D_FEATURE_LEVEL featureLevel;
        const D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_11_0 };

        DXGI_RATIONAL refreshRate = { 60, 1 };
        DXGI_MODE_DESC bufferDesc = {};
        bufferDesc.Width = 100;
        bufferDesc.Height = 100;
        bufferDesc.RefreshRate = refreshRate;
        bufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        bufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        bufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

        DXGI_SAMPLE_DESC sampleDesc = { 1, 0 };
        DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
        swapChainDesc.BufferDesc = bufferDesc;
        swapChainDesc.SampleDesc = sampleDesc;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = 1;
        swapChainDesc.OutputWindow = window;
        swapChainDesc.Windowed = 1;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

        IDXGISwapChain* swapChain;
        ID3D11Device* device;
        ID3D11DeviceContext* context;

        if (((long(__stdcall*)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL*, UINT, UINT, const DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**))(D3D11CreateDeviceAndSwapChain))
            (NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, featureLevels, 2, D3D11_SDK_VERSION, &swapChainDesc, &swapChain, &device, &featureLevel, &context) < 0) {
            DEBUG_LOG("D3D11CreateDeviceAndSwapChain failed");
            ::DestroyWindow(window);
            ::UnregisterClassA(windowClass.lpszClassName, windowClass.hInstance);
            return;
        }

        DEBUG_LOG("DX11 objects created, copying vtable");

        g_MethodsTable = (uint150_t*)::calloc(205, sizeof(uint150_t));
        ::memcpy(g_MethodsTable, *(uint150_t**)swapChain, 18 * sizeof(uint150_t));
        ::memcpy(g_MethodsTable + 18, *(uint150_t**)device, 43 * sizeof(uint150_t));
        ::memcpy(g_MethodsTable + 18 + 43, *(uint150_t**)context, 144 * sizeof(uint150_t));

        swapChain->Release();
        device->Release();
        context->Release();
        ::DestroyWindow(window);
        ::UnregisterClassA(windowClass.lpszClassName, windowClass.hInstance);

        DEBUG_LOG("DX11 method table initialized");
    }

    static HRESULT __stdcall DX11PresentHook(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
        if (Hook::s_Callbacks.OnPresent)
            Hook::s_Callbacks.OnPresent();

        return DX11PresentOriginal(pSwapChain, SyncInterval, Flags);
    }

    static DWORD WINAPI DX11InitThread(LPVOID lpParameter) {
        Sleep(100);

        DEBUG_LOG("DX11 init thread starting...");

        DX11Hook* hook = static_cast<DX11Hook*>(lpParameter);
        hook->InitializeMethodTable();

        if (!g_MethodsTable) {
            DEBUG_LOG("DX11 method table initialization failed");
            return 1;
        }

        DEBUG_LOG("Installing DX11 hooks...");
        INSTALL_HOOK_ADDRESS(DX11Present, g_MethodsTable[8]);

        MemoryManager::ApplyMod("DX11Present");

        DEBUG_LOG("DX11 installation complete");
        return 0;
    }

    bool DX11Hook::Install() {
        DEBUG_LOG("DX11 starting installation...");

        if (!GetModuleHandleW(L"d3d11.dll")) {
            DEBUG_LOG("d3d11.dll not loaded");
            return false;
        }

        DEBUG_LOG("d3d11.dll found, creating init thread...");
        CreateThread(NULL, 0, DX11InitThread, this, 0, NULL);

        return true;
    }

    void DX11Hook::Uninstall() {
        MemoryManager::RestoreAndEraseMod("DX11Present");

        if (g_MethodsTable) {
            free(g_MethodsTable);
            g_MethodsTable = nullptr;
        }
    }

}