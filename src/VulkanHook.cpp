#include "FrameJacker.h"
#include <DetourMacros.hpp>
#include <MemoryManager.h>
#if FRAMEJACKER_INCLUDE_VULKAN
#include "vulkan_core.h"
#endif

using namespace ByteWeaver;

namespace FrameJacker {
    typedef VkResult(VKAPI_PTR* PFN_vkCreateInstance_Custom)(const VkInstanceCreateInfo*, const VkAllocationCallbacks*, VkInstance*);
    typedef void (VKAPI_PTR* PFN_vkDestroyInstance_Custom)(VkInstance, const VkAllocationCallbacks*);
    typedef VkResult(VKAPI_PTR* PFN_vkEnumeratePhysicalDevices_Custom)(VkInstance, uint32_t*, VkPhysicalDevice*);
    typedef VkResult(VKAPI_PTR* PFN_vkCreateDevice_Custom)(VkPhysicalDevice, const VkDeviceCreateInfo*, const VkAllocationCallbacks*, VkDevice*);
    typedef void (VKAPI_PTR* PFN_vkDestroyDevice_Custom)(VkDevice, const VkAllocationCallbacks*);
    typedef PFN_vkVoidFunction(VKAPI_PTR* PFN_vkGetDeviceProcAddr_Custom)(VkDevice, const char*);

    DECLARE_HOOK(vkQueuePresentKHR, VkResult, __stdcall, __stdcall,
        VkQueue queue, const VkPresentInfoKHR* pPresentInfo);

    DECLARE_HOOK(vkAcquireNextImageKHR, VkResult, __stdcall, __stdcall,
        VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout,
        VkSemaphore semaphore, VkFence fence, uint32_t* pImageIndex);

    DECLARE_HOOK(vkCreateSwapchainKHR, VkResult, __stdcall, __stdcall,
        VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain);

    static uint150_t* g_MethodsTable = nullptr;
    static VkDevice g_FakeDevice = VK_NULL_HANDLE;
    static VkDevice g_Device = VK_NULL_HANDLE;
    static VkInstance g_Instance = VK_NULL_HANDLE;
    static VkPhysicalDevice g_PhysicalDevice = VK_NULL_HANDLE;
    static VkSwapchainKHR g_CurrentSwapchain = VK_NULL_HANDLE;
    static uint32_t g_CurrentImageIndex = 0;

    static VkResult __stdcall vkAcquireNextImageKHRHook(
        VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout,
        VkSemaphore semaphore, VkFence fence, uint32_t* pImageIndex) {

        g_Device = device;
        g_CurrentSwapchain = swapchain;

        VkResult result = vkAcquireNextImageKHROriginal(device, swapchain, timeout, semaphore, fence, pImageIndex);

        if (result == VK_SUCCESS) {
            g_CurrentImageIndex = *pImageIndex;
        }

        return result;
    }

    static VkResult __stdcall vkCreateSwapchainKHRHook(
        VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain) {

        if (Hook::s_Callbacks.OnResize)
            Hook::s_Callbacks.OnResize();

        return vkCreateSwapchainKHROriginal(device, pCreateInfo, pAllocator, pSwapchain);
    }

    static VkResult __stdcall vkQueuePresentKHRHook(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
        if (Hook::s_Callbacks.OnPresent)
            Hook::s_Callbacks.OnPresent();

        if (Hook::s_Callbacks.OnRender && g_Device) {
            RenderContext ctx = {};
            ctx.api = API::Vulkan;
            ctx.device = g_Device;
            ctx.commandBuffer = nullptr; 
            ctx.swapChain = (void*)g_CurrentSwapchain;
            ctx.imageIndex = g_CurrentImageIndex;
            ctx.extra = (void*)queue; 
            Hook::s_Callbacks.OnRender(ctx);
        }

        return vkQueuePresentKHROriginal(queue, pPresentInfo);
    }

    void VulkanHook::InitializeMethodTable() {
        DEBUG_LOG("Vulkan InitMethodTable starting...");

        VkInstanceCreateInfo instanceInfo = {};
        instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

        HMODULE libVulkan = ::GetModuleHandleW(L"vulkan-1.dll");
        if (!libVulkan) {
            DEBUG_LOG("vulkan-1.dll not found");
            return;
        }

        auto vkCreateInstance = (PFN_vkCreateInstance_Custom)::GetProcAddress(libVulkan, "vkCreateInstance");

        auto vkEnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices_Custom)::GetProcAddress(libVulkan, "vkEnumeratePhysicalDevices");
        auto vkCreateDevice = (PFN_vkCreateDevice_Custom)::GetProcAddress(libVulkan, "vkCreateDevice");
        auto vkDestroyDevice = (PFN_vkDestroyDevice_Custom)::GetProcAddress(libVulkan, "vkDestroyDevice");
        auto vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr_Custom)::GetProcAddress(libVulkan, "vkGetDeviceProcAddr");

        if (vkCreateInstance(&instanceInfo, nullptr, &g_Instance) != VK_SUCCESS) {
            DEBUG_LOG("vkCreateInstance failed");
            return;
        }

        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(g_Instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            DEBUG_LOG("No Vulkan devices found");
            vkDestroyInstance(g_Instance, nullptr);
            return;
        }




        VkPhysicalDevice devices[8];
        vkEnumeratePhysicalDevices(g_Instance, &deviceCount, devices);
        g_PhysicalDevice = devices[0];

        uint32_t queueFamily = 0;
        float queuePriority = 1.0f;

        VkDeviceQueueCreateInfo queueInfo = {};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = queueFamily;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;

        const char* swapchainExt = "VK_KHR_swapchain";
        VkDeviceCreateInfo deviceInfo = {};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.enabledExtensionCount = 1;
        deviceInfo.ppEnabledExtensionNames = &swapchainExt;

        if (vkCreateDevice(g_PhysicalDevice, &deviceInfo, nullptr, &g_FakeDevice) != VK_SUCCESS) {
            DEBUG_LOG("vkCreateDevice failed");
            vkDestroyInstance(g_Instance, nullptr);
            return;
        }

        void* acquireNextImageAddr = vkGetDeviceProcAddr(g_FakeDevice, "vkAcquireNextImageKHR");
        void* queuePresentAddr = vkGetDeviceProcAddr(g_FakeDevice, "vkQueuePresentKHR");
        void* createSwapchainAddr = vkGetDeviceProcAddr(g_FakeDevice, "vkCreateSwapchainKHR");

        if (!acquireNextImageAddr || !queuePresentAddr || !createSwapchainAddr) {
            DEBUG_LOG("Failed to get Vulkan function pointers");
            vkDestroyDevice(g_FakeDevice, nullptr);
            vkDestroyInstance(g_Instance, nullptr);
            return;
        }

        g_MethodsTable = (uint150_t*)::calloc(3, sizeof(uint150_t));
        g_MethodsTable[0] = (uint150_t)acquireNextImageAddr;
        g_MethodsTable[1] = (uint150_t)queuePresentAddr;
        g_MethodsTable[2] = (uint150_t)createSwapchainAddr;

        DEBUG_LOG("Vulkan function pointers obtained");

        vkDestroyDevice(g_FakeDevice, nullptr);
        g_FakeDevice = VK_NULL_HANDLE;

        DEBUG_LOG("Vulkan method table initialized");
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
        INSTALL_HOOK_ADDRESS(vkAcquireNextImageKHR, g_MethodsTable[0]);
        INSTALL_HOOK_ADDRESS(vkQueuePresentKHR, g_MethodsTable[1]);
        INSTALL_HOOK_ADDRESS(vkCreateSwapchainKHR, g_MethodsTable[2]);

        MemoryManager::ApplyMod("vkAcquireNextImageKHR");
        MemoryManager::ApplyMod("vkQueuePresentKHR");
        MemoryManager::ApplyMod("vkCreateSwapchainKHR");

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

        MemoryManager::RestoreAndEraseMod("vkAcquireNextImageKHR");
        MemoryManager::RestoreAndEraseMod("vkQueuePresentKHR");
        MemoryManager::RestoreAndEraseMod("vkCreateSwapchainKHR");
        HMODULE libVulkan = ::GetModuleHandleW(L"vulkan-1.dll");
        auto vkDestroyInstance = (PFN_vkDestroyInstance_Custom)::GetProcAddress(libVulkan, "vkDestroyInstance");
        if (g_Instance) {
            vkDestroyInstance(g_Instance, nullptr);
            g_Instance = VK_NULL_HANDLE;
        }

        if (g_MethodsTable) {
            free(g_MethodsTable);
            g_MethodsTable = nullptr;
        }
    }
}