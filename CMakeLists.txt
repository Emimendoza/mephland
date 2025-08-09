cmake_minimum_required(VERSION 3.10)
project(mephland LANGUAGES CXX)

SET(CMAKE_CXX_STANDARD 23)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED sdl3)
pkg_check_modules(Vulkan REQUIRED vulkan)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(mephland ${SOURCES})

target_compile_definitions(
        mephland PRIVATE
)

target_include_directories(mephland PRIVATE
        "src/include"
        ${SDL3_INCLUDE_DIRS}
        ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(
        mephland PRIVATE
        ${SDL3_LIBRARIES}
        ${Vulkan_LIBRARIES}
)