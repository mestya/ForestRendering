cmake_minimum_required (VERSION 3.13)

# ------------------------------------------------------------------------------
# 1. 基础环境
# ------------------------------------------------------------------------------
if (WIN32)
    set (LUGGCGL_TMP_DIR ${CMAKE_SOURCE_DIR}/../Temp)
    if (NOT EXISTS ${LUGGCGL_TMP_DIR})
        file (MAKE_DIRECTORY ${LUGGCGL_TMP_DIR})
    endif ()
    set (ENV{TMP} ${LUGGCGL_TMP_DIR})
endif ()

project (
    ForestScene
    VERSION 1.0.0
    LANGUAGES CXX C
)

set (CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo")
if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif ()
set_property (CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${CMAKE_CONFIGURATION_TYPES})

# ------------------------------------------------------------------------------
# 2. 依赖库配置
# ------------------------------------------------------------------------------
find_package (Git REQUIRED)
include (FetchContent)
set (FETCHCONTENT_BASE_DIR "${PROJECT_BINARY_DIR}/dependencies" CACHE PATH "Dependencies folder" FORCE)
if (NOT EXISTS "${FETCHCONTENT_BASE_DIR}")
    file (MAKE_DIRECTORY "${FETCHCONTENT_BASE_DIR}")
endif ()

# --- Assimp (使用 Homebrew 安装的) ---
find_package(assimp REQUIRED)

# --- GLFW (尝试使用下载脚本，如果报错，请改为 find_package(glfw3 REQUIRED)) ---
set (LUGGCGL_GLFW_MIN_VERSION 3.2.0)
set (LUGGCGL_GLFW_DOWNLOAD_VERSION 3.3.2)
include (CMake/InstallGLFW.cmake)
find_package (glfw3 ${LUGGCGL_GLFW_MIN_VERSION} REQUIRED)

# --- GLM (手动配置修正版) ---
set(GLM_ROOT "${CMAKE_SOURCE_DIR}/glm-0.9.9.5")

# 检查路径是否正确
if(NOT EXISTS "${GLM_ROOT}/glm/glm.hpp")
    message(FATAL_ERROR "GLM not found! looking at: ${GLM_ROOT}/glm/glm.hpp")
endif()

# 关键修正：创建一个名为 glm 的接口库，这样 bonobo 链接 glm 时才不会报错
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE ${GLM_ROOT})

# --- 其他小工具 ---
include (CMake/InstallTinyFileDialogs.cmake)
include (CMake/InstallSTB.cmake)
if (NOT EXISTS "${CMAKE_SOURCE_DIR}/res")
    include (CMake/RetrieveResourceArchive.cmake)
endif ()

# ------------------------------------------------------------------------------
# 3. 配置文件
# ------------------------------------------------------------------------------
set (MSAA_RATE "1" CACHE STRING "Window MSAA rate")
set (WIDTH "1280" CACHE STRING "Window width")
set (HEIGHT "720" CACHE STRING "Window height")
set (ROOT_DIR "${PROJECT_SOURCE_DIR}")

configure_file ("${PROJECT_SOURCE_DIR}/src/core/config.hpp.in" "${PROJECT_BINARY_DIR}/config.hpp")

# ------------------------------------------------------------------------------
# 4. 编译选项与核心库链接
# ------------------------------------------------------------------------------
add_library (CG_Labs_options INTERFACE)
target_compile_definitions(
    CG_Labs_options
    INTERFACE
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>>:NOMINMAX>
)
target_compile_options (
    CG_Labs_options
    INTERFACE
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>>:/utf-8;/Zc:__cplusplus>
)
target_compile_features (CG_Labs_options INTERFACE cxx_std_14)

add_library (assignment_setup INTERFACE)
target_link_libraries (
    assignment_setup
    INTERFACE
        bonobo
        CG_Labs_options
        tinyfiledialogs::tinyfiledialogs
)

# Windows 辅助函数 (保留)
function (copy_dlls target target_folder)
    if (WIN32)
        get_target_property (assimp_dll_path assimp::assimp IMPORTED_LOCATION_RELEASE)
        get_filename_component (assimp_dll "${assimp_dll_path}" NAME)
        if (NOT EXISTS "${target_folder}/${assimp_dll}")
            add_custom_command (
                TARGET ${target}
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy ${assimp_dll_path} ${target_folder}
                COMMENT "Copying dependent DLLs"
            )
        endif ()
    endif ()
endfunction ()

# ------------------------------------------------------------------------------
# 5. 构建子目录
# ------------------------------------------------------------------------------
add_subdirectory ("${CMAKE_SOURCE_DIR}/src/external")
add_subdirectory ("${CMAKE_SOURCE_DIR}/src/core")

# ------------------------------------------------------------------------------
# 6. 定义你的项目
# ------------------------------------------------------------------------------
set(MAIN_SOURCE "src/main.cpp")

set(SCENE_SOURCE 
    "src/ForestScene.cpp" 
    "src/ForestScene.hpp"
)

add_executable(ForestScene 
    ${MAIN_SOURCE}
    ${SCENE_SOURCE} 
)

target_link_libraries(ForestScene
    PRIVATE
    assignment_setup
    glfw
    glm              # 现在这个名字有效了，因为我们在上面定义了 add_library(glm)
    assimp::assimp   # 使用更安全的命名空间链接
)

# ------------------------------------------------------------------------------
# 7. 资源复制
# ------------------------------------------------------------------------------
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/shaders DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
set(FORESTSCENE_RES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/res")
if (NOT EXISTS "${FORESTSCENE_RES_DIR}")
    set(FORESTSCENE_RES_DIR "${FETCHCONTENT_BASE_DIR}/resources")
endif ()
file(COPY ${FORESTSCENE_RES_DIR} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

message(STATUS "ForestScene setup done.")

# ------------------------------------------------------------------------------
# 8. Vulkan 版本 (框架搭建阶段)
# ------------------------------------------------------------------------------
option(FORESTSCENE_ENABLE_VULKAN "Build Vulkan framework executable" ON)
if (FORESTSCENE_ENABLE_VULKAN)
    find_package(Vulkan REQUIRED)

    add_executable(ForestSceneVK
        "src/main_vk.cpp"
        "src/vulkan/App.cpp"
        "src/vulkan/App.hpp"
        "src/vulkan/Context.cpp"
        "src/vulkan/Context.hpp"
        "src/vulkan/vk_common.hpp"
    )

    target_link_libraries(ForestSceneVK
        PRIVATE
            CG_Labs_options
            glfw
            Vulkan::Vulkan
    )

    target_compile_definitions(ForestSceneVK PRIVATE GLFW_INCLUDE_NONE)
    target_compile_features(ForestSceneVK PRIVATE cxx_std_17)
endif ()
