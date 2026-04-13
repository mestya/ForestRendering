add_library (bonobo)
target_sources (
	bonobo
	PUBLIC
		${CMAKE_SOURCE_DIR}/src/core/Bonobo.h
		${CMAKE_SOURCE_DIR}/src/core/BuildSettings.h
		"${CMAKE_BINARY_DIR}/config.hpp"
		${CMAKE_SOURCE_DIR}/src/core/FPSCamera.h
		${CMAKE_SOURCE_DIR}/src/core/FPSCamera.inl
		${CMAKE_SOURCE_DIR}/src/core/helpers.hpp
		${CMAKE_SOURCE_DIR}/src/core/InputHandler.h
		${CMAKE_SOURCE_DIR}/src/core/Log.h
		${CMAKE_SOURCE_DIR}/src/core/LogView.h
		${CMAKE_SOURCE_DIR}/src/core/node.hpp
		${CMAKE_SOURCE_DIR}/src/core/opengl.hpp
		${CMAKE_SOURCE_DIR}/src/core/ShaderProgramManager.hpp
		${CMAKE_SOURCE_DIR}/src/core/TRSTransform.h
		${CMAKE_SOURCE_DIR}/src/core/TRSTransform.inl
		${CMAKE_SOURCE_DIR}/src/core/various.hpp
		${CMAKE_SOURCE_DIR}/src/core/WindowManager.hpp
		${CMAKE_SOURCE_DIR}/src/core/parametric_shapes.hpp
	PRIVATE
		${CMAKE_SOURCE_DIR}/src/core/Bonobo.cpp
		${CMAKE_SOURCE_DIR}/src/core/helpers.cpp
		${CMAKE_SOURCE_DIR}/src/core/InputHandler.cpp
		${CMAKE_SOURCE_DIR}/src/core/Log.cpp
		${CMAKE_SOURCE_DIR}/src/core/LogView.cpp
		${CMAKE_SOURCE_DIR}/src/core/node.cpp
		${CMAKE_SOURCE_DIR}/src/core/opengl.cpp
		${CMAKE_SOURCE_DIR}/src/core/ShaderProgramManager.cpp
		${CMAKE_SOURCE_DIR}/src/core/various.cpp
		${CMAKE_SOURCE_DIR}/src/core/WindowManager.cpp
		${CMAKE_SOURCE_DIR}/src/core/parametric_shapes.cpp
)

target_include_directories (
	bonobo
	PUBLIC
		"${CMAKE_SOURCE_DIR}/src"
		"${CMAKE_BINARY_DIR}"
		"${ASSIMP_INCLUDE_DIRS}"
		"${CMAKE_SOURCE_DIR}/src/external"
)
message("xxxxxxxxxxxyyyyyyyyyy:${CMAKE_SOURCE_DIR}/src/external")
message("xxxxxxxxxxxyyyyyyyyyy:${CMAKE_SOURCE_DIR}")
message("xxxxxxxxxxxyyyyyyyyyy:${CMAKE_BINARY_DIR}")
message("xxxxxxxxxxxyyyyyyyyyy:${ASSIMP_INCLUDE_DIRS}")

target_link_libraries (
	bonobo
	PUBLIC
		${ASSIMP_LIBRARIES}
		external_libs
		glfw
		glm::glm
		${CMAKE_DL_LIBS}
	PRIVATE
		CG_Labs_options
		stb::stb
)

install (TARGETS bonobo DESTINATION lib)
