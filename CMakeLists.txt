cmake_minimum_required(VERSION 3.28)
project(mephland LANGUAGES CXX C)


set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS ON)
set(GLSLC_FLAGS "-O" "--target-env=vulkan1.3")
add_compile_options(--embed-dir=${CMAKE_BINARY_DIR}/shaders -Wno-c23-extensions)

include(CheckIncludeFileCXX)

option(USE_REFLECTION "Use experimental c++ reflection" OFF)
option(OPTIMIZE "Enable optimizations" OFF)
option(SDL_BACKEND "Compile with SDL backend" ON)


if (USE_REFLECTION)
	CHECK_INCLUDE_FILE_CXX("experimental/reflection" HAVE_EXPERIMENTAL_REFLECTION)
	if (NOT HAVE_EXPERIMENTAL_REFLECTION)
		message(FATAL_ERROR "Experimental reflection is not supported by your compiler")
	endif()
	add_compile_definitions(MLAND_USE_REFLECTION)
endif()

if (OPTIMIZE)
	add_compile_options(-Os)
endif()

find_package(PkgConfig REQUIRED)
find_package(Libdrm REQUIRED)

pkg_check_modules(VULKAN REQUIRED vulkan)
pkg_check_modules(WAYLAND_SERVER REQUIRED wayland-server)

if (SDL_BACKEND)
	add_compile_definitions(MLAND_SDL_BACKEND)
	pkg_check_modules(SDL3 REQUIRED sdl3)
else ()
	set(SDL3_LIBRARIES "")
	set(SDL3_INCLUDE_DIRS "")
endif()
file(GLOB_RECURSE SOURCES "impl/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h")
file(GLOB_RECURSE TEMPLATES "headers/*.tcc")
file(GLOB_RECURSE C_SOURCES "impl/*.c")

add_executable(mephland ${SOURCES} ${HEADERS} ${TEMPLATES} ${C_SOURCES})

target_include_directories(mephland PRIVATE
		include
		${VULKAN_INCLUDE_DIRS}
		${Libdrm_INCLUDE_DIRS}
		${SDL3_INCLUDE_DIRS}
		${WAYLAND_SERVER_INCLUDE_DIRS}
)
target_link_libraries(mephland PRIVATE
		${VULKAN_LIBRARIES}
		${Libdrm_LIBRARIES}
		${SDL3_LIBRARIES}
		${WAYLAND_SERVER_LIBRARIES}
)

add_custom_command(
		OUTPUT ${CMAKE_BINARY_DIR}/shaders
		COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
		COMMENT "Creating shaders output directory"
)

file(GLOB_RECURSE SHADERS "shaders/*")
foreach(SHADER ${SHADERS})
	get_filename_component(SHADER_NAME ${SHADER} NAME)
	add_custom_command(
			OUTPUT ${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.spv
			COMMAND glslc ${GLSLC_FLAGS} ${SHADER} -o ${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.spv
			DEPENDS ${SHADER}
			COMMENT "Compiling shader ${SHADER_NAME}"
	)
	list(APPEND SPV_SHADERS ${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.spv)
endforeach()
add_custom_target(compile_shaders ALL DEPENDS ${CMAKE_BINARY_DIR}/shaders ${SPV_SHADERS})
add_dependencies(mephland compile_shaders)