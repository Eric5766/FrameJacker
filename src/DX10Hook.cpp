#include "FrameJacker.h"
#include <DetourMacros.hpp>
#include <MemoryManager.h>
#if FRAMEJACKER_INCLUDE_D3D10
#include <dxgi.h>
#include <d3d10.h>
#endif

using namespace ByteWeaver;

namespace FrameJacker {

    DECLARE_HOOK(DX10Present, HRESULT, __stdcall, __stdcall,
        IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);

    static uint150_t* g_MethodsTable = nullptr;
    static IDXGISwapChain* g_SwapChain = nullptr;
    static ID3D10Device* g_Device = nullptr;

    void DX10Hook::InitializeMethodTable() {
        DEBUG_LOG("DX10 InitMethodTable starting...");

        WNDCLASSEXA windowClass = {};
        windowClass.cbSize = sizeof(WNDCLASSEXA);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = DefWindowProc;
        windowClass.hInstance = GetModuleHandle(NULL);
        windowClass.lpszClassName = "FrameJackerDX10";

        ::RegisterClassExA(&windowClass);
        HWND window = ::CreateWindowA(windowClass.lpszClassName, "FrameJacker DX10", WS_OVERLAPPEDWINDOW,
            0, 0, 100, 100, NULL, NULL, windowClass.hInstance, NULL);

        HMODULE libDXGI = ::GetModuleHandleW(L"dxgi.dll");
        HMODULE libD3D10 = ::GetModuleHandleW(L"d3d10.dll");

        if (!libDXGI || !libD3D10) {
            DEBUG_LOG("dxgi.dll or d3d10.dll not found");
            ::DestroyWindow(window);
            ::UnregisterClassA(windowClass.lpszClassName, windowClass.hInstance);
            return;
        }

        void* CreateDXGIFactory = ::GetProcAddress(libDXGI, "CreateDXGIFactory");
        if (!CreateDXGIFactory) {
            DEBUG_LOG("CreateDXGIFactory not found");
            ::DestroyWindow(window);
            ::UnregisterClassA(windowClass.lpszClassName, windowClass.hInstance);
            return;
        }

        IDXGIFactory* factory;
        if (((long(__stdcall*)(const IID&, void**))(CreateDXGIFactory))(__uuidof(IDXGIFactory), (void**)&factory) < 0) {
            DEBUG_LOG("CreateDXGIFactory failed");
            ::DestroyWindow(window);
            ::UnregisterClassA(windowClass.lpszClassName, windowClass.hInstance);
            return;
        }

        IDXGIAdapter* adapter;
        if (factory->EnumAdapters(0, &adapter) == DXGI_ERROR_NOT_FOUND) {
            DEBUG_LOG("EnumAdapters failed");
            factory->Release();
            ::DestroyWindow(window);
            ::UnregisterClassA(windowClass.lpszClassName, windowClass.hInstance);
            return;
        }

        void* D3D10CreateDeviceAndSwapChain = ::GetProcAddress(libD3D10, "D3D10CreateDeviceAndSwapChain");
        if (!D3D10CreateDeviceAndSwapChain) {
            DEBUG_LOG("D3D10CreateDeviceAndSwapChain not found");
            adapter->Release();
            factory->Release();
            ::DestroyWindow(window);
            ::UnregisterClassA(windowClass.lpszClassName, windowClass.hInstance);
            return;
        }

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
        ID3D10Device* device;

        if (((long(__stdcall*)(IDXGIAdapter*, D3D10_DRIVER_TYPE, HMODULE, UINT, UINT, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**, ID3D10Device**))(D3D10CreateDeviceAndSwapChain))
            (adapter, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0, D3D10_SDK_VERSION, &swapChainDesc, &swapChain, &device) < 0) {
            DEBUG_LOG("D3D10CreateDeviceAndSwapChain failed");
            adapter->Release();
            factory->Release();
            ::DestroyWindow(window);
            ::UnregisterClassA(windowClass.lpszClassName, windowClass.hInstance);
            return;
        }

        DEBUG_LOG("DX10 objects created, copying vtable");

        g_MethodsTable = (uint150_t*)::calloc(116, sizeof(uint150_t));
        ::memcpy(g_MethodsTable, *(uint150_t**)swapChain, 18 * sizeof(uint150_t));
        ::memcpy(g_MethodsTable + 18, *(uint150_t**)device, 98 * sizeof(uint150_t));

        swapChain->Release();
        device->Release();
        adapter->Release();
        factory->Release();
        ::DestroyWindow(window);
        ::UnregisterClassA(windowClass.lpszClassName, windowClass.hInstance);

        DEBUG_LOG("DX10 method table initialized");
    }

    static HRESULT __stdcall DX10PresentHook(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
        g_SwapChain = pSwapChain;

        if (!g_Device && pSwapChain) {
            pSwapChain->GetDevice(__uuidof(ID3D10Device), (void**)&g_Device);
        }

        if (Hook::s_Callbacks.OnPresent)
            Hook::s_Callbacks.OnPresent();

        if (Hook::s_Callbacks.OnRender && g_Device) {
            RenderContext ctx = {};
            ctx.api = API::D3D10;
            ctx.device = g_Device;
            ctx.commandBuffer = nullptr;
            ctx.swapChain = pSwapChain;
            ctx.renderTarget = nullptr;
            ctx.imageIndex = 0;
            ctx.extra = nullptr;

            Hook::s_Callbacks.OnRender(ctx);
        }

        return DX10PresentOriginal(pSwapChain, SyncInterval, Flags);
    }

    static DWORD WINAPI DX10InitThread(LPVOID lpParameter) {
        Sleep(100);

        DEBUG_LOG("DX10 init thread starting...");

        DX10Hook* hook = static_cast<DX10Hook*>(lpParameter);
        hook->InitializeMethodTable();

        if (!g_MethodsTable) {
            DEBUG_LOG("DX10 method table initialization failed");
            return 1;
        }

        DEBUG_LOG("Installing DX10 hooks...");
        INSTALL_HOOK_ADDRESS(DX10Present, g_MethodsTable[8]);

        MemoryManager::ApplyMod("DX10Present");

        DEBUG_LOG("DX10 installation complete");
        return 0;
    }

    bool DX10Hook::Install() {
        DEBUG_LOG("DX10 starting installation...");

        if (!GetModuleHandleW(L"d3d10.dll")) {
            DEBUG_LOG("d3d10.dll not loaded");
            return false;
        }

        DEBUG_LOG("d3d10.dll found, creating init thread...");
        CreateThread(NULL, 0, DX10InitThread, this, 0, NULL);

        return true;
    }

    void DX10Hook::Uninstall() {
        MemoryManager::RestoreAndEraseMod("DX10Present");

        if (g_Device) {
            g_Device->Release();
            g_Device = nullptr;
        }

        if (g_MethodsTable) {
            free(g_MethodsTable);
            g_MethodsTable = nullptr;
        }
    }

}