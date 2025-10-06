#include "FrameJacker.h"
#include <DetourMacros.hpp>
#include <MemoryManager.h>
#if FRAMEJACKER_INCLUDE_OPENGL
#include <Windows.h>
#include <gl/GL.h>
#endif

using namespace ByteWeaver;

namespace FrameJacker {

    DECLARE_HOOK(wglSwapBuffers, BOOL, __stdcall, __stdcall, HDC hdc);

    static uint150_t* g_MethodsTable = nullptr;
    static HDC g_HDC = nullptr;

    void OpenGLHook::InitializeMethodTable() {
        DEBUG_LOG("OpenGL InitMethodTable starting...");

        HMODULE libOpenGL32 = ::GetModuleHandleW(L"opengl32.dll");
        if (!libOpenGL32) {
            DEBUG_LOG("opengl32.dll not found");
            return;
        }

        g_MethodsTable = (uint150_t*)::calloc(1, sizeof(uint150_t));

        void* wglSwapBuffersAddr = ::GetProcAddress(libOpenGL32, "wglSwapBuffers");
        if (!wglSwapBuffersAddr) {
            DEBUG_LOG("wglSwapBuffers not found");
            free(g_MethodsTable);
            g_MethodsTable = nullptr;
            return;
        }

        g_MethodsTable[0] = (uint150_t)wglSwapBuffersAddr;

        DEBUG_LOG("OpenGL method table initialized");
    }

    static BOOL __stdcall wglSwapBuffersHook(HDC hdc) {
        g_HDC = hdc;

        if (Hook::s_Callbacks.OnPresent)
            Hook::s_Callbacks.OnPresent();

        if (Hook::s_Callbacks.OnRender) {
            RenderContext ctx = {};
            ctx.api = API::OpenGL;
            ctx.device = nullptr;
            ctx.commandBuffer = nullptr;
            ctx.swapChain = nullptr;
            ctx.renderTarget = nullptr;
            ctx.imageIndex = 0;
            ctx.extra = hdc;  // Pass HDC in extra

            Hook::s_Callbacks.OnRender(ctx);
        }

        return wglSwapBuffersOriginal(hdc);
    }

    static DWORD WINAPI OpenGLInitThread(LPVOID lpParameter) {
        Sleep(100);

        DEBUG_LOG("OpenGL init thread starting...");

        OpenGLHook* hook = static_cast<OpenGLHook*>(lpParameter);
        hook->InitializeMethodTable();

        if (!g_MethodsTable) {
            DEBUG_LOG("OpenGL method table initialization failed");
            return 1;
        }

        DEBUG_LOG("Installing OpenGL hooks...");
        INSTALL_HOOK_ADDRESS(wglSwapBuffers, g_MethodsTable[0]);

        MemoryManager::ApplyMod("wglSwapBuffers");

        DEBUG_LOG("OpenGL installation complete");
        return 0;
    }

    bool OpenGLHook::Install() {
        DEBUG_LOG("OpenGL starting installation...");

        if (!GetModuleHandleW(L"opengl32.dll")) {
            DEBUG_LOG("opengl32.dll not loaded");
            return false;
        }

        DEBUG_LOG("opengl32.dll found, creating init thread...");
        CreateThread(NULL, 0, OpenGLInitThread, this, 0, NULL);

        return true;
    }

    void OpenGLHook::Uninstall() {
        MemoryManager::RestoreAndEraseMod("wglSwapBuffers");

        if (g_MethodsTable) {
            free(g_MethodsTable);
            g_MethodsTable = nullptr;
        }
    }

}