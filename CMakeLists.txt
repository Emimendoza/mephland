cmake_minimum_required(VERSION 3.28)
project(mephland LANGUAGES CXX C)


set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS ON)
set(GLSLC_FLAGS "-O" "--target-env=vulkan1.3")
add_compile_options(--embed-dir=${CMAKE_BINARY_DIR}/shaders -Wno-c23-extensions)

include(CheckIncludeFileCXX)

option(USE_REFLECTION "Use experimental c++ reflection" OFF)
option(SDL_BACKEND "Compile with SDL backend" ON)


if (USE_REFLECTION)
	CHECK_INCLUDE_FILE_CXX("experimental/reflection" HAVE_EXPERIMENTAL_REFLECTION)
	if (NOT HAVE_EXPERIMENTAL_REFLECTION)
		message(FATAL_ERROR "Experimental reflection is not supported by your compiler")
	endif()
	add_compile_definitions(MLAND_USE_REFLECTION)
endif()

find_package(PkgConfig REQUIRED)
find_package(Libdrm REQUIRED)

# find Vulkan SDK
find_package( Vulkan REQUIRED )

# Require Vulkan version ≥ 1.3.256 (earliest version when the Vulkan module was available)
if( ${Vulkan_VERSION} VERSION_LESS "1.3.256" )
	message( FATAL_ERROR "Minimum required Vulkan version for C++ modules is 1.3.256. "
			"Found ${Vulkan_VERSION}."
	)
endif()

# set up Vulkan C++ module as a library
add_library( VulkanHppModule )
target_sources( VulkanHppModule PUBLIC
		FILE_SET CXX_MODULES
		BASE_DIRS ${Vulkan_INCLUDE_DIR}
		FILES ${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm
)
target_compile_features( VulkanHppModule PUBLIC cxx_std_20 )
target_compile_definitions( VulkanHppModule PRIVATE
)
target_link_libraries( VulkanHppModule PUBLIC Vulkan::Headers )

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
		${Libdrm_INCLUDE_DIRS}
		${SDL3_INCLUDE_DIRS}
		${WAYLAND_SERVER_INCLUDE_DIRS}
)
target_link_libraries(mephland PRIVATE
		VulkanHppModule
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