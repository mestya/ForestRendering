cmake_minimum_required (VERSION 3.10)

# Workaround for the lab computers
if (WIN32)
	set (LUGGCGL_TMP_DIR ${CMAKE_SOURCE_DIR}/../Temp)
	if (NOT EXISTS ${LUGGCGL_TMP_DIR})
		file (MAKE_DIRECTORY ${LUGGCGL_TMP_DIR})
	endif ()
	set (ENV{TMP} ${LUGGCGL_TMP_DIR})
endif ()


# Rendering backend build switch.
# OPENGL: build only OpenGL executable
# VULKAN: build only Vulkan executable
# BOTH:   build both executables
set (RENDER_BACKEND "BOTH" CACHE STRING "Rendering backend to build: OPENGL, VULKAN, BOTH")
set_property (CACHE RENDER_BACKEND PROPERTY STRINGS OPENGL VULKAN BOTH)

string (TOUPPER "${RENDER_BACKEND}" RENDER_BACKEND)
if (NOT RENDER_BACKEND STREQUAL "OPENGL" AND NOT RENDER_BACKEND STREQUAL "VULKAN" AND NOT RENDER_BACKEND STREQUAL "BOTH")
	message (FATAL_ERROR "Invalid RENDER_BACKEND='${RENDER_BACKEND}'. Expected OPENGL, VULKAN, or BOTH.")
endif ()

set (BUILD_OPENGL OFF)
set (BUILD_VULKAN OFF)
if (RENDER_BACKEND STREQUAL "OPENGL")
	set (BUILD_OPENGL ON)
elseif (RENDER_BACKEND STREQUAL "VULKAN")
	set (BUILD_VULKAN ON)
else ()
	set (BUILD_OPENGL ON)
	set (BUILD_VULKAN ON)
endif ()

# Set project attributes.
project (
	CG_Labs
	VERSION 2021.2
	DESCRIPTION [[Repository for Computer Graphics courses EDAF80 and EDAN35 at Lund University, Sweden.]]
	HOMEPAGE_URL [[https://github.com/LUGGPublic/CG_Labs/]]
	LANGUAGES CXX C
)


# Set build type
set (CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo")
if (NOT CMAKE_BUILD_TYPE)
	set (
		CMAKE_BUILD_TYPE
		"Release"
		CACHE STRING
		"Choose the type of build, options are: ${CMAKE_CONFIGURATION_TYPES}."
		FORCE
	)
endif (NOT CMAKE_BUILD_TYPE)
set_property (CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${CMAKE_CONFIGURATION_TYPES})


# if (BUILD_VULKAN)
	find_package(Vulkan REQUIRED)
	# find_package(glfw3 CONFIG REQUIRED)
# endif ()

# To be able to dowload and compile the dependencies
find_package (Git REQUIRED)
include (FetchContent)
# FetchContent base directory
#
# Historically this project forced FETCHCONTENT_BASE_DIR to live inside the
# source tree ("${PROJECT_SOURCE_DIR}/dependencies"). That makes builds fragile
# when switching generators (e.g. Xcode ↔ Unix Makefiles), because CMake keeps
# generator-specific caches in that directory.
#
# Default to a per-build-directory location to avoid generator/cache conflicts.
set (_LUGGCGL_LEGACY_FETCHCONTENT_BASE_DIR "${PROJECT_SOURCE_DIR}/dependencies")
set (_LUGGCGL_DEFAULT_FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/_deps")

if (DEFINED FETCHCONTENT_BASE_DIR AND FETCHCONTENT_BASE_DIR STREQUAL "${_LUGGCGL_LEGACY_FETCHCONTENT_BASE_DIR}")
	message (STATUS "Migrating FETCHCONTENT_BASE_DIR from in-source '${FETCHCONTENT_BASE_DIR}' to '${_LUGGCGL_DEFAULT_FETCHCONTENT_BASE_DIR}'")
	set (FETCHCONTENT_BASE_DIR "${_LUGGCGL_DEFAULT_FETCHCONTENT_BASE_DIR}" CACHE PATH "Base folder where dependencies are processed." FORCE)
else ()
	set (FETCHCONTENT_BASE_DIR "${_LUGGCGL_DEFAULT_FETCHCONTENT_BASE_DIR}" CACHE PATH "Base folder where dependencies are processed.")
endif ()

if (NOT EXISTS "${FETCHCONTENT_BASE_DIR}")
	file (MAKE_DIRECTORY "${FETCHCONTENT_BASE_DIR}")
endif ()

unset (_LUGGCGL_LEGACY_FETCHCONTENT_BASE_DIR)
unset (_LUGGCGL_DEFAULT_FETCHCONTENT_BASE_DIR)

# assimp is used for loading the objects’s models
set (LUGGCGL_ASSIMP_MIN_VERSION 5.1.0)
set (LUGGCGL_ASSIMP_DOWNLOAD_VERSION 5.1.2)
include (CMake/InstallAssimp.cmake)
find_package (assimp ${LUGGCGL_ASSIMP_MIN_VERSION} REQUIRED)
link_directories (${ASSIMP_LIBRARY_DIRS})

# assimp's exported target links against 'minizip' on macOS when found via
# pkg-config, but does not always propagate the corresponding library search
# path to consumers. Add it explicitly so the final link step can resolve it.
if (APPLE)
	set (_luggcgl_minizip_library_dirs "")

	find_package (PkgConfig QUIET)
	if (PKG_CONFIG_FOUND)
		pkg_check_modules (LUGGCGL_MINIZIP QUIET minizip)
		if (LUGGCGL_MINIZIP_FOUND AND LUGGCGL_MINIZIP_LIBRARY_DIRS)
			set (_luggcgl_minizip_library_dirs ${LUGGCGL_MINIZIP_LIBRARY_DIRS})
		endif ()
	endif ()

	if (NOT _luggcgl_minizip_library_dirs)
		find_library (LUGGCGL_MINIZIP_LIBRARY NAMES minizip PATHS /opt/homebrew/lib /usr/local/lib)
		if (LUGGCGL_MINIZIP_LIBRARY)
			get_filename_component (_luggcgl_minizip_library_dirs "${LUGGCGL_MINIZIP_LIBRARY}" DIRECTORY)
		endif ()
	endif ()

	if (_luggcgl_minizip_library_dirs)
		link_directories (${_luggcgl_minizip_library_dirs})
	endif ()

	unset (_luggcgl_minizip_library_dirs)
endif ()

# GLFW is used for inputs and windows handling
set (LUGGCGL_GLFW_MIN_VERSION 3.2.0)
set (LUGGCGL_GLFW_DOWNLOAD_VERSION 3.3.2)
include (CMake/InstallGLFW.cmake)
find_package (glfw3 ${LUGGCGL_GLFW_MIN_VERSION} REQUIRED)

# GLM is used for matrices, vectors and camera handling
if (EXISTS "${CMAKE_SOURCE_DIR}/glm-0.9.9.5/glm/glm.hpp")
	# GLM is header-only; prefer the vendored copy to avoid platform-specific package config issues.
	add_library(luggcgl_glm INTERFACE)
	target_include_directories(luggcgl_glm INTERFACE "${CMAKE_SOURCE_DIR}/glm-0.9.9.5")
	add_library(glm::glm ALIAS luggcgl_glm)
else ()
	set (LUGGCGL_GLM_DOWNLOAD_VERSION 0.9.9.5)
	include (CMake/InstallGLM.cmake)
	find_package (glm REQUIRED)
endif ()

# TinyFileDialogs is used for displaying error popups.
include (CMake/InstallTinyFileDialogs.cmake)

# stb is used for loading in image files.
include (CMake/InstallSTB.cmake)

# Resources are found in an external archive
include (CMake/RetrieveResourceArchive.cmake)


# Configure *C++ Environment Variables*
set (MSAA_RATE "1" CACHE STRING "Window MSAA rate")
set (WIDTH "1600" CACHE STRING "Window width")
set (HEIGHT "900" CACHE STRING "Window height")
set (ROOT_DIR "${PROJECT_SOURCE_DIR}")
configure_file ("${PROJECT_SOURCE_DIR}/src/core/config.hpp.in" "${PROJECT_BINARY_DIR}/config.hpp")


# Set up Doxygen documentation generation
option (LUGGCGL_BUILD_DOCUMENTATION "Build documentation for Lund University Computer Graphics Labs" OFF)
if (LUGGCGL_BUILD_DOCUMENTATION)
	find_package (Doxygen REQUIRED)
	set (LUGGCGL_VERSION_NUMBER "${CMAKE_PROJECT_VERSION}")
	set (LUGGCGL_DOC_INPUT "${CMAKE_SOURCE_DIR}/src")
	set (LUGGCGL_DOC_EXCLUDE ${CMAKE_SOURCE_DIR}/src/external)
	set (LUGGCGL_DOC_OUTPUT "${CMAKE_BINARY_DIR}/doc")
	if (NOT EXISTS "${LUGGCGL_DOC_OUTPUT}")
		file (MAKE_DIRECTORY "${LUGGCGL_DOC_OUTPUT}")
	endif ()
	configure_file ("${CMAKE_SOURCE_DIR}/Doxyfile.in" "${CMAKE_BINARY_DIR}/doc/Doxyfile" @ONLY)
	add_custom_target (
		doc
		COMMAND ${DOXYGEN_EXECUTABLE} "${CMAKE_BINARY_DIR}/doc/Doxyfile"
		WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/doc"
		COMMENT "Generating API documentation with Doxygen"
	)
endif ()


# Define a “fake” library to store the C++ configuration:
# all libraries and executables linking against this target will automatically
# inherit its configuration such as C++ standard version and additional C++
# flags.
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


# Define another “fake” library that provides a common setup for all
# assignments. At the moment it only contains all common dependencies but it
# could contain more, like specific compilation flags.
add_library (assignment_setup INTERFACE)
target_link_libraries (
       assignment_setup
       INTERFACE
               bonobo
               CG_Labs_options
               tinyfiledialogs::tinyfiledialogs
)


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


add_subdirectory ("${CMAKE_SOURCE_DIR}/src/external")
add_subdirectory ("${CMAKE_SOURCE_DIR}/src/core")
set(MAIN_SOURCE "${CMAKE_SOURCE_DIR}/src/main.cpp")
# set(VK_MAIN_SOURCE "${CMAKE_SOURCE_DIR}/src/main_vk.cpp")

set(VK_SOURCE
	"${CMAKE_SOURCE_DIR}/src/vk_main.cpp"
	"${CMAKE_SOURCE_DIR}/src/vk_main.hpp"
)

# Vulkan framework skeleton (final integration architecture).
file(GLOB_RECURSE VK_FRAMEWORK_SOURCES CONFIGURE_DEPENDS
	"${CMAKE_SOURCE_DIR}/src/vk/*.cpp"
	"${CMAKE_SOURCE_DIR}/src/vk/*.hpp"
)

set(SCENE_SOURCE 
    "${CMAKE_SOURCE_DIR}/src/ForestScene.cpp" 
    "${CMAKE_SOURCE_DIR}/src/ForestScene.hpp"
)

if (BUILD_OPENGL)
	add_executable(ForestScene
		${MAIN_SOURCE}
		${SCENE_SOURCE}
	)

	target_compile_definitions(ForestScene
		PRIVATE
		BUILD_VULKAN=0
	)

	target_link_libraries(ForestScene
		PRIVATE
		assignment_setup
	)

	copy_dlls (ForestScene "${CMAKE_CURRENT_BINARY_DIR}")
	install (
		TARGETS ForestScene
		DESTINATION ${CMAKE_SOURCE_DIR}/src/bin
	)
endif ()

if (BUILD_VULKAN)
	find_program(SLANGC_EXECUTABLE NAMES slangc REQUIRED)
	# find_program(GLSLC_EXECUTABLE NAMES glslc HINTS ENV PATH)
	if (NOT SLANGC_EXECUTABLE)
	message(FATAL_ERROR "slangc not found")
	endif()

	set(VK_SHADER_SOURCES
	"${CMAKE_SOURCE_DIR}/shaders/vk/mesh.vert.slang"
	"${CMAKE_SOURCE_DIR}/shaders/vk/mesh.frag.slang"
	)

	set(VK_SHADER_SPV_OUTPUTS "")
	foreach(shader_src IN LISTS VK_SHADER_SOURCES)
	get_filename_component(shader_name "${shader_src}" NAME)
	set(shader_out "${CMAKE_CURRENT_BINARY_DIR}/res/vk/${shader_name}.spv")
	add_custom_command(
		OUTPUT "${shader_out}"
		COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/res/vk"
		COMMAND "${SLANGC_EXECUTABLE}" -o "${shader_out}" "${shader_src}"
		DEPENDS "${shader_src}"
		VERBATIM
	)
	list(APPEND VK_SHADER_SPV_OUTPUTS "${shader_out}")
	endforeach()

	add_custom_target(vk_shaders DEPENDS ${VK_SHADER_SPV_OUTPUTS})

	add_executable(ForestSceneVk
		${MAIN_SOURCE}
		${VK_SOURCE}
		${VK_FRAMEWORK_SOURCES}
	)
	add_dependencies(ForestSceneVk vk_shaders)

	target_compile_definitions(ForestSceneVk
		PRIVATE
		BUILD_VULKAN=1
	)

	target_include_directories(ForestSceneVk
		PRIVATE
		"${CMAKE_SOURCE_DIR}/src"
	)
	target_compile_features(ForestSceneVk PRIVATE cxx_std_20)


	add_definitions(-DNOMINMAX)
		# 5. 添加必要的宏定义（针对 Vulkan-Hpp）
	target_compile_definitions(ForestSceneVk PRIVATE 
			VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
			VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
		)

	target_link_libraries(ForestSceneVk
		PRIVATE
		CG_Labs_options
		assignment_setup
		glfw
		Vulkan::Vulkan
		stb::stb
		
		${CMAKE_DL_LIBS}
		
	)

	install (
		TARGETS ForestSceneVk
		DESTINATION ${CMAKE_SOURCE_DIR}/src/bin
	)
endif ()

copy_dlls (ForestScene "${CMAKE_CURRENT_BINARY_DIR}")
message(${CMAKE_CURRENT_BINARY_DIR})
message(${CMAKE_SOURCE_DIR})
install (
	TARGETS
	DESTINATION ${CMAKE_SOURCE_DIR}/src/bin
)


# ------------------------------------------------------------------------------
# 7. 资源复制
# ------------------------------------------------------------------------------
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/shaders DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Copy downloaded resources (if any) into the runtime `res/` folder first,
# then copy the project's tracked `res/` folder to override/extend.
if (DEFINED resources_SOURCE_DIR AND EXISTS "${resources_SOURCE_DIR}")
	file(COPY "${resources_SOURCE_DIR}/" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/res")
endif ()
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/res DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

message(STATUS "ForestScene setup done.")

# # install (DIRECTORY ${CMAKE_SOURCE_DIR}/shaders DESTINATION bin)
# # install (DIRECTORY ${CMAKE_SOURCE_DIR}/res DESTINATION bin)
