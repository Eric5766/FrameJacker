#include "FrameJacker.h"
#include <DetourMacros.hpp>
#include <MemoryManager.h>
#if FRAMEJACKER_INCLUDE_D3D12
#include <d3d12.h>
#include <dxgi.h>
#include <dxgi1_4.h>
#endif
using namespace ByteWeaver;

namespace FrameJacker {

    DECLARE_HOOK(DX12Present, HRESULT, __stdcall, __stdcall,
        IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags);

    DECLARE_HOOK(DX12ExecuteCommandLists, void, __stdcall, __stdcall,
        ID3D12CommandQueue* queue, UINT NumCommandLists, ID3D12CommandList** ppCommandLists);

    DECLARE_HOOK(DX12ResizeBuffers, HRESULT, __stdcall, __stdcall,
        IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height,
        DXGI_FORMAT NewFormat, UINT SwapChainFlags);

    static ID3D12CommandQueue* g_CommandQueue = nullptr;
    static IDXGISwapChain3* g_SwapChain = nullptr;
    static uint150_t* g_MethodsTable = nullptr;

    static DWORD WINAPI InitThread(LPVOID lpParameter) {
        Sleep(100);

        DX12Hook* hook = static_cast<DX12Hook*>(lpParameter);

        DEBUG_LOG("Init thread starting...");

        hook->InitializeMethodTable();

        if (!g_MethodsTable) {
            DEBUG_LOG("Method table initialization failed");
            return 1;
        }

        DEBUG_LOG("Installing hooks...");
        INSTALL_HOOK_ADDRESS(DX12ExecuteCommandLists, g_MethodsTable[54]);
        INSTALL_HOOK_ADDRESS(DX12ResizeBuffers, g_MethodsTable[140 - 132]);
        INSTALL_HOOK_ADDRESS(DX12Present, g_MethodsTable[140]);

        MemoryManager::ApplyMod("DX12ExecuteCommandLists");
        MemoryManager::ApplyMod("DX12ResizeBuffers");
        MemoryManager::ApplyMod("DX12Present");

        DEBUG_LOG("Installation complete");
        return 0;
    }

    void DX12Hook::InitializeMethodTable() {
        DEBUG_LOG("InitMethodTable starting...");

        WNDCLASSEXA windowClass = {};
        windowClass.cbSize = sizeof(WNDCLASSEXA);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = DefWindowProc;
        windowClass.cbClsExtra = 0;
        windowClass.cbWndExtra = 0;
        windowClass.hInstance = GetModuleHandle(NULL);
        windowClass.hIcon = NULL;
        windowClass.hCursor = NULL;
        windowClass.hbrBackground = NULL;
        windowClass.lpszMenuName = NULL;
        windowClass.lpszClassName = "FrameJackerDX12";
        windowClass.hIconSm = NULL;

        if (!::RegisterClassExA(&windowClass)) {
            DEBUG_LOG("RegisterClassExA failed: %d", GetLastError());
            return;
        }

        HWND window = ::CreateWindowA(windowClass.lpszClassName, "FrameJacker DirectX Window", WS_OVERLAPPEDWINDOW,
            0, 0, 100, 100, NULL, NULL, windowClass.hInstance, NULL);

        if (!window) {
            DEBUG_LOG("CreateWindow failed: %d", GetLastError());
            ::UnregisterClassA(windowClass.lpszClassName, windowClass.hInstance);
            return;
        }

        DEBUG_LOG("Window created successfully");

        HMODULE libDXGI = ::GetModuleHandleW(L"dxgi.dll");
        HMODULE libD3D12 = ::GetModuleHandleW(L"d3d12.dll");

        DEBUG_LOG("libDXGI: %p, libD3D12: %p", libDXGI, libD3D12);

        if (!libDXGI || !libD3D12)
        {
            DEBUG_LOG("Modules not found");
            ::DestroyWindow(window);
            ::UnregisterClassA(windowClass.lpszClassName, windowClass.hInstance);
            return;
        }

        void* CreateDXGIFactory;
        DEBUG_LOG("CreateDXGIFactory address: %p", CreateDXGIFactory);

        if ((CreateDXGIFactory = ::GetProcAddress(libDXGI, "CreateDXGIFactory")) == NULL)
        {
            DEBUG_LOG("GetProcAddress CreateDXGIFactory failed");
            ::DestroyWindow(window);
            ::UnregisterClassA(windowClass.lpszClassName, windowClass.hInstance);
            return;
        }

        IDXGIFactory* factory;
        if (((long(__stdcall*)(const IID&, void**))(CreateDXGIFactory))(__uuidof(IDXGIFactory), (void**)&factory) < 0)
        {
            DEBUG_LOG("CreateDXGIFactory failed");
            ::DestroyWindow(window);
            ::UnregisterClassA(windowClass.lpszClassName, windowClass.hInstance);

            return;
        }

        IDXGIAdapter* adapter;
        if (factory->EnumAdapters(0, &adapter) == DXGI_ERROR_NOT_FOUND)
        {
            DEBUG_LOG("EnumAdapters failed");
            ::DestroyWindow(window);
            ::UnregisterClassA(windowClass.lpszClassName, windowClass.hInstance);
            return;
        }

        void* D3D12CreateDevice;
        if ((D3D12CreateDevice = ::GetProcAddress(libD3D12, "D3D12CreateDevice")) == NULL)
        {
            DEBUG_LOG("GetProcAddress D3D12CreateDevice failed");
            ::DestroyWindow(window);
            ::UnregisterClassA(windowClass.lpszClassName, windowClass.hInstance);
            return;
        }

        ID3D12Device* device;
        if (((long(__stdcall*)(IUnknown*, D3D_FEATURE_LEVEL, const IID&, void**))(D3D12CreateDevice))(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&device) < 0)
        {
            DEBUG_LOG("D3D12CreateDevice failed");
            ::DestroyWindow(window);
            ::UnregisterClassA(windowClass.lpszClassName, windowClass.hInstance);
            return;
        }

        DEBUG_LOG("Device created");

        D3D12_COMMAND_QUEUE_DESC queueDesc;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueDesc.Priority = 0;
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.NodeMask = 0;

        ID3D12CommandQueue* commandQueue;
        if (device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), (void**)&commandQueue) < 0)
        {
            DEBUG_LOG("CreateCommandQueue failed");
            ::DestroyWindow(window);
            ::UnregisterClassA(windowClass.lpszClassName, windowClass.hInstance);
            return;
        }

        ID3D12CommandAllocator* commandAllocator;
        if (device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&commandAllocator) < 0)
        {
            DEBUG_LOG("CreateCommandAllocator failed");
            ::DestroyWindow(window);
            ::UnregisterClassA(windowClass.lpszClassName, windowClass.hInstance);
            return;
        }

        ID3D12GraphicsCommandList* commandList;
        if (device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, NULL, __uuidof(ID3D12GraphicsCommandList), (void**)&commandList) < 0)
        {
            DEBUG_LOG("CreateCommandList failed");
            ::DestroyWindow(window);
            ::UnregisterClassA(windowClass.lpszClassName, windowClass.hInstance);
            return;
        }

        DXGI_RATIONAL refreshRate;
        refreshRate.Numerator = 60;
        refreshRate.Denominator = 1;

        DXGI_MODE_DESC bufferDesc;
        bufferDesc.Width = 100;
        bufferDesc.Height = 100;
        bufferDesc.RefreshRate = refreshRate;
        bufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        bufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        bufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

        DXGI_SAMPLE_DESC sampleDesc;
        sampleDesc.Count = 1;
        sampleDesc.Quality = 0;

        DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
        swapChainDesc.BufferDesc = bufferDesc;
        swapChainDesc.SampleDesc = sampleDesc;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = 2;
        swapChainDesc.OutputWindow = window;
        swapChainDesc.Windowed = 1;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

        IDXGISwapChain* swapChain;
        if (factory->CreateSwapChain(commandQueue, &swapChainDesc, &swapChain) < 0)
        {
            DEBUG_LOG("CreateSwapChain failed");
            ::DestroyWindow(window);
            ::UnregisterClassA(windowClass.lpszClassName, windowClass.hInstance);
            return;
        }

        DEBUG_LOG("All D3D12 objects created, copying vtable");

        g_MethodsTable = (uint150_t*)::calloc(150, sizeof(uint150_t));
        ::memcpy(g_MethodsTable, *(uint150_t**)device, 44 * sizeof(uint150_t));
        ::memcpy(g_MethodsTable + 44, *(uint150_t**)commandQueue, 19 * sizeof(uint150_t));
        ::memcpy(g_MethodsTable + 44 + 19, *(uint150_t**)commandAllocator, 9 * sizeof(uint150_t));
        ::memcpy(g_MethodsTable + 44 + 19 + 9, *(uint150_t**)commandList, 60 * sizeof(uint150_t));
        ::memcpy(g_MethodsTable + 44 + 19 + 9 + 60, *(uint150_t**)swapChain, 18 * sizeof(uint150_t));

        device->Release();
        commandQueue->Release();
        commandAllocator->Release();
        commandList->Release();
        swapChain->Release();

        ::DestroyWindow(window);
        ::UnregisterClassA(windowClass.lpszClassName, windowClass.hInstance);

        DEBUG_LOG("Method table initialized successfully");
    }

    static HRESULT __stdcall DX12PresentHook(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags) {
        g_SwapChain = pSwapChain;

        if (Hook::s_Callbacks.OnPresent)
            Hook::s_Callbacks.OnPresent();

        if (Hook::s_Callbacks.OnRender && g_CommandQueue) {
            RenderContext ctx = {};
            ctx.api = API::D3D12;
            ctx.device = nullptr;  // DX12 device accessible via command queue
            ctx.commandBuffer = nullptr;  // User creates their own command lists
            ctx.swapChain = pSwapChain;
            ctx.renderTarget = nullptr;
            ctx.imageIndex = 0;
            ctx.extra = g_CommandQueue;  // Pass command queue

            Hook::s_Callbacks.OnRender(ctx);
        }

        return DX12PresentOriginal(pSwapChain, SyncInterval, Flags);
    }

    static HRESULT __stdcall DX12ResizeBuffersHook(
        IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height,
        DXGI_FORMAT NewFormat, UINT SwapChainFlags) {

        if (Hook::s_Callbacks.OnResize)
            Hook::s_Callbacks.OnResize();

        return DX12ResizeBuffersOriginal(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
    }

    static void __stdcall DX12ExecuteCommandListsHook(ID3D12CommandQueue* queue,
        UINT NumCommandLists, ID3D12CommandList** ppCommandLists) {

        if (!g_CommandQueue) {
            g_CommandQueue = queue;
            if (Hook::s_Callbacks.OnDeviceCreated)
                Hook::s_Callbacks.OnDeviceCreated(queue);
        }

        DX12ExecuteCommandListsOriginal(queue, NumCommandLists, ppCommandLists);
    }

    bool DX12Hook::Install() {
        DEBUG_LOG("Starting installation...");

        if (!GetModuleHandleW(L"d3d12.dll")) {
            DEBUG_LOG("d3d12.dll not loaded");
            return false;
        }

        DEBUG_LOG("d3d12.dll found, creating init thread...");
        CreateThread(NULL, 0, InitThread, this, 0, NULL);

        return true;
    }

    void DX12Hook::Uninstall() {
        MemoryManager::RestoreAndEraseMod("DX12Present");
        MemoryManager::RestoreAndEraseMod("DX12ExecuteCommandLists");
        MemoryManager::RestoreAndEraseMod("DX12ResizeBuffers");

        if (g_MethodsTable) {
            free(g_MethodsTable);
            g_MethodsTable = nullptr;
        }
    }

}