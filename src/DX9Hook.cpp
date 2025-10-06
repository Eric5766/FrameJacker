#include "FrameJacker.h"
#include <DetourMacros.hpp>
#include <MemoryManager.h>
#if FRAMEJACKER_INCLUDE_D3D9
#include <d3d9.h>
#endif

using namespace ByteWeaver;

namespace FrameJacker {

    DECLARE_HOOK(DX9EndScene, HRESULT, __stdcall, __stdcall, LPDIRECT3DDEVICE9 pDevice);
    DECLARE_HOOK(DX9Reset, HRESULT, __stdcall, __stdcall, LPDIRECT3DDEVICE9 pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters);

    static uint150_t* g_MethodsTable = nullptr;
    static LPDIRECT3DDEVICE9 g_Device = nullptr;

    void DX9Hook::InitializeMethodTable() {
        DEBUG_LOG("DX9 InitMethodTable starting...");

        WNDCLASSEXA windowClass = {};
        windowClass.cbSize = sizeof(WNDCLASSEXA);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = DefWindowProc;
        windowClass.hInstance = GetModuleHandle(NULL);
        windowClass.lpszClassName = "FrameJackerDX9";

        ::RegisterClassExA(&windowClass);
        HWND window = ::CreateWindowA(windowClass.lpszClassName, "FrameJacker DX9", WS_OVERLAPPEDWINDOW,
            0, 0, 100, 100, NULL, NULL, windowClass.hInstance, NULL);

        HMODULE libD3D9 = ::GetModuleHandleW(L"d3d9.dll");
        if (!libD3D9) {
            DEBUG_LOG("d3d9.dll not found");
            ::DestroyWindow(window);
            ::UnregisterClassA(windowClass.lpszClassName, windowClass.hInstance);
            return;
        }

        void* Direct3DCreate9 = ::GetProcAddress(libD3D9, "Direct3DCreate9");
        if (!Direct3DCreate9) {
            DEBUG_LOG("Direct3DCreate9 not found");
            ::DestroyWindow(window);
            ::UnregisterClassA(windowClass.lpszClassName, windowClass.hInstance);
            return;
        }

        LPDIRECT3D9 direct3D9 = ((LPDIRECT3D9(__stdcall*)(uint32_t))(Direct3DCreate9))(D3D_SDK_VERSION);
        if (!direct3D9) {
            DEBUG_LOG("Direct3DCreate9 failed");
            ::DestroyWindow(window);
            ::UnregisterClassA(windowClass.lpszClassName, windowClass.hInstance);
            return;
        }

        D3DPRESENT_PARAMETERS params = {};
        params.BackBufferWidth = 0;
        params.BackBufferHeight = 0;
        params.BackBufferFormat = D3DFMT_UNKNOWN;
        params.BackBufferCount = 0;
        params.MultiSampleType = D3DMULTISAMPLE_NONE;
        params.MultiSampleQuality = 0;
        params.SwapEffect = D3DSWAPEFFECT_DISCARD;
        params.hDeviceWindow = window;
        params.Windowed = 1;
        params.EnableAutoDepthStencil = 0;
        params.AutoDepthStencilFormat = D3DFMT_UNKNOWN;
        params.Flags = 0;
        params.FullScreen_RefreshRateInHz = 0;
        params.PresentationInterval = 0;

        LPDIRECT3DDEVICE9 device;
        if (direct3D9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF, window,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_DISABLE_DRIVER_MANAGEMENT, &params, &device) < 0) {
            DEBUG_LOG("CreateDevice failed");
            direct3D9->Release();
            ::DestroyWindow(window);
            ::UnregisterClassA(windowClass.lpszClassName, windowClass.hInstance);
            return;
        }

        DEBUG_LOG("DX9 device created, copying vtable");

        g_MethodsTable = (uint150_t*)::calloc(119, sizeof(uint150_t));
        ::memcpy(g_MethodsTable, *(uint150_t**)device, 119 * sizeof(uint150_t));

        device->Release();
        direct3D9->Release();
        ::DestroyWindow(window);
        ::UnregisterClassA(windowClass.lpszClassName, windowClass.hInstance);

        DEBUG_LOG("DX9 method table initialized");
    }

    static HRESULT __stdcall DX9EndSceneHook(LPDIRECT3DDEVICE9 pDevice) {
        g_Device = pDevice;

        if (Hook::s_Callbacks.OnPresent)
            Hook::s_Callbacks.OnPresent();

        if (Hook::s_Callbacks.OnRender) {
            RenderContext ctx = {};
            ctx.api = API::D3D9;
            ctx.device = pDevice;
            ctx.commandBuffer = nullptr;
            ctx.swapChain = nullptr;
            ctx.renderTarget = nullptr;
            ctx.imageIndex = 0;
            ctx.extra = nullptr;

            Hook::s_Callbacks.OnRender(ctx);
        }

        return DX9EndSceneOriginal(pDevice);
    }

    static HRESULT __stdcall DX9ResetHook(LPDIRECT3DDEVICE9 pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters) {
        if (Hook::s_Callbacks.OnResize)
            Hook::s_Callbacks.OnResize();

        return DX9ResetOriginal(pDevice, pPresentationParameters);
    }

    static DWORD WINAPI DX9InitThread(LPVOID lpParameter) {
        Sleep(100);

        DEBUG_LOG("DX9 init thread starting...");

        DX9Hook* hook = static_cast<DX9Hook*>(lpParameter);
        hook->InitializeMethodTable();

        if (!g_MethodsTable) {
            DEBUG_LOG("DX9 method table initialization failed");
            return 1;
        }

        DEBUG_LOG("Installing DX9 hooks...");
        INSTALL_HOOK_ADDRESS(DX9EndScene, g_MethodsTable[42]);
        INSTALL_HOOK_ADDRESS(DX9Reset, g_MethodsTable[16]);

        MemoryManager::ApplyMod("DX9EndScene");
        MemoryManager::ApplyMod("DX9Reset");

        DEBUG_LOG("DX9 installation complete");
        return 0;
    }

    bool DX9Hook::Install() {
        DEBUG_LOG("DX9 starting installation...");

        if (!GetModuleHandleW(L"d3d9.dll")) {
            DEBUG_LOG("d3d9.dll not loaded");
            return false;
        }

        DEBUG_LOG("d3d9.dll found, creating init thread...");
        CreateThread(NULL, 0, DX9InitThread, this, 0, NULL);

        return true;
    }

    void DX9Hook::Uninstall() {
        MemoryManager::RestoreAndEraseMod("DX9EndScene");
        MemoryManager::RestoreAndEraseMod("DX9Reset");

        if (g_MethodsTable) {
            free(g_MethodsTable);
            g_MethodsTable = nullptr;
        }
    }

}