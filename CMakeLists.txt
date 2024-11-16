cmake_minimum_required(VERSION 3.28)
project(mephland LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

find_package(PkgConfig REQUIRED)

pkg_check_modules(VULKAN REQUIRED vulkan)

file(GLOB_RECURSE SOURCES "impl/*.cpp")
file(GLOB_RECURSE HEADERS "headers/*.h")

add_executable(mephland ${SOURCES} ${HEADERS})

target_include_directories(mephland PRIVATE
        ${VULKAN_INCLUDE_DIRS}
)
target_link_libraries(mephland PRIVATE
        ${VULKAN_LIBRARIES}
)