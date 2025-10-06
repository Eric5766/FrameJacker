cmake_minimum_required(VERSION 3.20)
project(FrameJacker VERSION 0.1.1)

set(CMAKE_CXX_STANDARD 20)

option(FRAMEJACKER_D3D9 "Enable D3D9 support" ON)
option(FRAMEJACKER_D3D10 "Enable D3D10 support" ON)
option(FRAMEJACKER_D3D11 "Enable D3D11 support" ON)
option(FRAMEJACKER_D3D12 "Enable D3D12 support" ON)
option(FRAMEJACKER_OPENGL "Enable OpenGL support" ON)
option(FRAMEJACKER_VULKAN "Enable Vulkan support" ON)
option(FRAMEJACKER_USE_FALLBACK_HEADERS "Use fallback DirectX headers for non-MSVC environments" OFF)

include(FetchContent)
FetchContent_Declare(
    ByteWeaver
    GIT_REPOSITORY https://github.com/0xKate/ByteWeaver.git
    GIT_TAG 1.0.46
)
FetchContent_MakeAvailable(ByteWeaver)

set(FRAMEJACKER_SOURCES src/FrameJacker.cpp)
set(FRAMEJACKER_LIBS ByteWeaver::ByteWeaver)
set(SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/api")

if(FRAMEJACKER_D3D9)
    list(APPEND FRAMEJACKER_SOURCES src/DX9Hook.cpp)
endif()

if(FRAMEJACKER_D3D10)
    list(APPEND FRAMEJACKER_SOURCES src/DX10Hook.cpp)
endif()

if(FRAMEJACKER_D3D11)
    list(APPEND FRAMEJACKER_SOURCES src/DX11Hook.cpp)
endif()

if(FRAMEJACKER_D3D12)
    list(APPEND FRAMEJACKER_SOURCES src/DX12Hook.cpp)
endif()

if(FRAMEJACKER_OPENGL)
    list(APPEND FRAMEJACKER_SOURCES src/OpenGLHook.cpp)
endif()

if(FRAMEJACKER_VULKAN)
    list(APPEND FRAMEJACKER_SOURCES src/VulkanHook.cpp)
endif()

add_library(FrameJacker STATIC ${FRAMEJACKER_SOURCES})

if(FRAMEJACKER_USE_FALLBACK_HEADERS OR NOT MSVC)
    target_include_directories(FrameJacker PUBLIC
        $<BUILD_INTERFACE:${SDK_DIR}/fallback>
    )
endif()

target_include_directories(FrameJacker PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${SDK_DIR}/include>
)

target_compile_definitions(FrameJacker PUBLIC
    FRAMEJACKER_INCLUDE_D3D9=$<BOOL:${FRAMEJACKER_D3D9}>
    FRAMEJACKER_INCLUDE_D3D10=$<BOOL:${FRAMEJACKER_D3D10}>
    FRAMEJACKER_INCLUDE_D3D11=$<BOOL:${FRAMEJACKER_D3D11}>
    FRAMEJACKER_INCLUDE_D3D12=$<BOOL:${FRAMEJACKER_D3D12}>
    FRAMEJACKER_INCLUDE_OPENGL=$<BOOL:${FRAMEJACKER_OPENGL}>
    FRAMEJACKER_INCLUDE_VULKAN=$<BOOL:${FRAMEJACKER_VULKAN}>
)

target_link_libraries(FrameJacker PUBLIC ${FRAMEJACKER_LIBS})

if(MSVC)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        target_compile_definitions(FrameJacker PRIVATE _AMD64_)
    else()
        target_compile_definitions(FrameJacker PRIVATE _X86_)
    endif()
endif()
