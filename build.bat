@echo off
setlocal

REM 创建 build 目录
if not exist build (
    mkdir build
)

REM 生成 VS2022 x64 工程
@REM cmake -S . -B build -DRENDER_BACKEND=OPENGL
@REM cmake -S . -B build -DRENDER_BACKEND=VULKAN
@REM cmake -S . -B build -DRENDER_BACKEND=BOTH
set VULKAN_SDK=D:/Setup/VulkanSDK
set VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation
set VK_LAYER_SETTINGS_PATH=VULKAN_SDK/Config
set VK_LOADER_DEBUG=error,warn

:: 查看变量
echo %VULKAN_SDK%
@REM slangc .\res\09_shader_base.slang -target spirv -o .\res\09_shader_base.spv
@REM slangc .\shaders\vk\mesh.frag.slang -target spirv -o .\res\vk\mesh.frag.spv 
@REM slangc .\shaders\vk\mesh.vert.slang -target spirv -o .\res\vk\mesh.vert.spv
@REM cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="D:\Code\xyRenderEngine\vcpkg\scripts\buildsystems\vcpkg.cmake"

cmake -S . -B build -G "Visual Studio 18 2026" -A x64  -DRENDER_BACKEND=VULKAN
@REM cmake -S . -B build -G "Visual Studio 18 2026" -A x64  -DRENDER_BACKEND=OPENGL
if errorlevel 1 (
    echo CMake generate failed
    exit /b 1
)

REM 编译 Release

cmake --build build --config Release 
if errorlevel 1 (
    echo Build failed
    exit /b 1
)

echo Build Success!
pause
