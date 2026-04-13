add_library (external_libs)
message("xxxxxxxxxxxyyyyyyycccccccc:${CMAKE_CURRENT_BINARY_DIR}")
message("xxxxxxxxxxxyyyyyyycccccccc:${CMAKE_SOURCE_DIR}")

target_sources(
    external_libs
    PUBLIC
        "${CMAKE_SOURCE_DIR}/src/external/Dear ImGui/imconfig.h"
        "${CMAKE_SOURCE_DIR}/src/external/Dear ImGui/imgui.h"
        "${CMAKE_SOURCE_DIR}/src/external/Dear ImGui/imgui_internal.h"
        "${CMAKE_SOURCE_DIR}/src/external/Dear ImGui/imgui_impl_glfw.h"
        "${CMAKE_SOURCE_DIR}/src/external/Dear ImGui/imgui_impl_opengl3.h"
        "${CMAKE_SOURCE_DIR}/src/external/Dear ImGui/imgui_impl_opengl3_loader.h"
        "${CMAKE_SOURCE_DIR}/src/external/Dear ImGui/imstb_rectpack.h"
        "${CMAKE_SOURCE_DIR}/src/external/Dear ImGui/imstb_textedit.h"
        "${CMAKE_SOURCE_DIR}/src/external/Dear ImGui/imstb_truetype.h"
        "${CMAKE_SOURCE_DIR}/src/external/GLAD/glad/glad.h"
        "${CMAKE_SOURCE_DIR}/src/external/GLAD/KHR/khrplatform.h"
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src/external/Dear ImGui/imgui.cpp"
        "${CMAKE_SOURCE_DIR}/src/external/Dear ImGui/imgui_demo.cpp"
        "${CMAKE_SOURCE_DIR}/src/external/Dear ImGui/imgui_draw.cpp"
        "${CMAKE_SOURCE_DIR}/src/external/Dear ImGui/imgui_tables.cpp"
        "${CMAKE_SOURCE_DIR}/src/external/Dear ImGui/imgui_widgets.cpp"
        "${CMAKE_SOURCE_DIR}/src/external/Dear ImGui/imgui_impl_glfw.cpp"
        "${CMAKE_SOURCE_DIR}/src/external/Dear ImGui/imgui_impl_opengl3.cpp"
        "${CMAKE_SOURCE_DIR}/src/external/GLAD/glad.c"
)

target_include_directories (
	external_libs
	SYSTEM
	PUBLIC
	    "${CMAKE_SOURCE_DIR}/src/external"
		"${CMAKE_SOURCE_DIR}/src/external/Dear ImGui"
		"${CMAKE_SOURCE_DIR}/src/external/GLAD"
)

target_link_libraries (
	external_libs
	PRIVATE
		glfw
		${CMAKE_DL_LIBS}
)

install (TARGETS external_libs DESTINATION lib)
