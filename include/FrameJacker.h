#pragma once
#include <functional>
#include <memory>

namespace FrameJacker {
    #ifdef _WIN64
        typedef uint64_t uint150_t;
    #else
        typedef uint32_t uint150_t;
    #endif


    extern bool g_EnableDebugLogging;
    using LogFunction = void(*)(const char* msg);
    extern LogFunction g_CustomLogHandler;

    void SetDebugLogging(bool enabled);
    void SetLogHandler(LogFunction handler);

    enum class API {
        Auto,
        D3D9,
        D3D10,
        D3D11,
        D3D12,
        OpenGL,
        Vulkan
    };

    struct RenderContext {
        API api;
        void* device;           // IDirect3DDevice9*, ID3D11Device*, VkDevice, etc.
        void* commandBuffer;    // ID3D12GraphicsCommandList*, VkCommandBuffer, etc.
        void* swapChain;        // IDXGISwapChain*, VkSwapchainKHR, etc.
        void* renderTarget;     // API-specific render target
        uint32_t imageIndex;    // For Vulkan/DX12 multi-buffering
        void* extra;            // API-specific extra data if needed
    };

    struct Callbacks {
        std::function<void()> OnPresent;
        std::function<void()> OnResize;
        std::function<void(void*)> OnDeviceCreated;
        std::function<void(const RenderContext&)> OnRender; 
    };

    class IGraphicsHook {
    public:
        virtual ~IGraphicsHook() = default;
        virtual bool Install() = 0;
        virtual void Uninstall() = 0;
        virtual API GetAPI() const = 0;
    };

    class Hook {
    public:
        static bool Initialize(API api = API::Auto);
        static void Shutdown();
        static void SetCallbacks(const Callbacks& callbacks);
        static API GetActiveAPI();
        ;
        static Callbacks s_Callbacks; 

    private:
        static std::unique_ptr<IGraphicsHook> s_ActiveHook;
    };

#define DEBUG_LOG(fmt, ...) \
    do { \
        if (FrameJacker::g_EnableDebugLogging) { \
            char buffer[512]; \
            snprintf(buffer, sizeof(buffer), "[FrameJacker] " fmt, ##__VA_ARGS__); \
            if (FrameJacker::g_CustomLogHandler) { \
                FrameJacker::g_CustomLogHandler(buffer); \
            } else { \
                printf("%s\n", buffer); \
                fflush(stdout); \
            } \
        } \
    } while(0)

#define DECLARE_GRAPHICS_HOOK(ClassName, APIEnum) \
    class ClassName : public IGraphicsHook { \
    public: \
        bool Install() override; \
        void Uninstall() override; \
        API GetAPI() const override { return API::APIEnum; } \
        void InitializeMethodTable(); \
    };


#if FRAMEJACKER_INCLUDE_D3D9
DECLARE_GRAPHICS_HOOK(DX9Hook, D3D9)
#endif

#if FRAMEJACKER_INCLUDE_D3D10
DECLARE_GRAPHICS_HOOK(DX10Hook, D3D10)
#endif

#if FRAMEJACKER_INCLUDE_D3D11
DECLARE_GRAPHICS_HOOK(DX11Hook, D3D11)
#endif

#if FRAMEJACKER_INCLUDE_D3D12
DECLARE_GRAPHICS_HOOK(DX12Hook, D3D12)
#endif

#if FRAMEJACKER_INCLUDE_OPENGL
DECLARE_GRAPHICS_HOOK(OpenGLHook, OpenGL)
#endif

#if FRAMEJACKER_INCLUDE_VULKAN
DECLARE_GRAPHICS_HOOK(VulkanHook, Vulkan)
#endif
   
}



