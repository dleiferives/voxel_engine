cmake_minimum_required(VERSION 3.20)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMake")
set(CMAKE_C_COMPILER "clang" CACHE STRING "C compiler" FORCE)
set(CMAKE_CXX_COMPILER "clang++" CACHE STRING "C++ compiler" FORCE)
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "0e5b6991-d74f-4b36-bd3f-8588d04d3f5d")
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(ENABLE_CPP20_MODULE ON)
add_compile_options(-fembed-bitcode)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(voxel LANGUAGES C CXX)

# Use C++23

# Options

# Dependencies
add_subdirectory(external/glfw)
target_link_libraries(glfw)
find_package(Vulkan REQUIRED)


add_executable(voxel
    src/main.cpp
)

target_compile_definitions(voxel PRIVATE
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
    VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
    # VULKAN_HPP_CXX_MODULE_EXPERIMENTAL_WARNING # Optional: silences experimental warning
)

target_include_directories(voxel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glm_tmp
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(voxel PRIVATE
    glfw
    Vulkan::Vulkan
)
