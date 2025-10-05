#include "FrameJacker.h"
#include <DetourMacros.hpp>
#include <MemoryManager.h>
#if FRAMEJACKER_INCLUDE_VULKAN
#include <vulkan_core.h>
#endif

using namespace ByteWeaver;

namespace FrameJacker {

    DECLARE_HOOK(vkQueuePresentKHR, VkResult, __stdcall, __stdcall,
        VkQueue queue, const VkPresentInfoKHR* pPresentInfo);

    static uint150_t* g_MethodsTable = nullptr;

    void VulkanHook::InitializeMethodTable() {
        DEBUG_LOG("Vulkan InitMethodTable starting...");

        HMODULE libVulkan = ::GetModuleHandleW(L"vulkan-1.dll");
        if (!libVulkan) {
            DEBUG_LOG("vulkan-1.dll not found");
            return;
        }

        g_MethodsTable = (uint150_t*)::calloc(1, sizeof(uint150_t));

        void* vkGetInstanceProcAddr = ::GetProcAddress(libVulkan, "vkGetInstanceProcAddr");
        if (!vkGetInstanceProcAddr) {
            DEBUG_LOG("vkGetInstanceProcAddr not found");
            free(g_MethodsTable);
            g_MethodsTable = nullptr;
            return;
        }

        PFN_vkGetInstanceProcAddr fpGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)vkGetInstanceProcAddr;
        PFN_vkQueuePresentKHR fpQueuePresentKHR = (PFN_vkQueuePresentKHR)fpGetInstanceProcAddr(VK_NULL_HANDLE, "vkQueuePresentKHR");

        if (!fpQueuePresentKHR) {
            DEBUG_LOG("vkQueuePresentKHR not found");
            free(g_MethodsTable);
            g_MethodsTable = nullptr;
            return;
        }

        g_MethodsTable[0] = (uint150_t)fpQueuePresentKHR;

        DEBUG_LOG("Vulkan method table initialized");
    }

    static VkResult __stdcall vkQueuePresentKHRHook(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
        if (Hook::s_Callbacks.OnPresent)
            Hook::s_Callbacks.OnPresent();

        return vkQueuePresentKHROriginal(queue, pPresentInfo);
    }

    static DWORD WINAPI VulkanInitThread(LPVOID lpParameter) {
        Sleep(100);

        DEBUG_LOG("Vulkan init thread starting...");

        VulkanHook* hook = static_cast<VulkanHook*>(lpParameter);
        hook->InitializeMethodTable();

        if (!g_MethodsTable) {
            DEBUG_LOG("Vulkan method table initialization failed");
            return 1;
        }

        DEBUG_LOG("Installing Vulkan hooks...");
        INSTALL_HOOK_ADDRESS(vkQueuePresentKHR, g_MethodsTable[0]);

        MemoryManager::ApplyMod("vkQueuePresentKHR");

        DEBUG_LOG("Vulkan installation complete");
        return 0;
    }

    bool VulkanHook::Install() {
        DEBUG_LOG("Vulkan starting installation...");

        if (!GetModuleHandleW(L"vulkan-1.dll")) {
            DEBUG_LOG("vulkan-1.dll not loaded");
            return false;
        }

        DEBUG_LOG("vulkan-1.dll found, creating init thread...");
        CreateThread(NULL, 0, VulkanInitThread, this, 0, NULL);

        return true;
    }

    void VulkanHook::Uninstall() {
        MemoryManager::RestoreAndEraseMod("vkQueuePresentKHR");

        if (g_MethodsTable) {
            free(g_MethodsTable);
            g_MethodsTable = nullptr;
        }
    }

}