cmake_minimum_required(VERSION 3.20)
set(CMAKE_C_COMPILER "clang" CACHE STRING "C compiler" FORCE)
set(CMAKE_CXX_COMPILER "clang++" CACHE STRING "C++ compiler" FORCE)
project(voxel LANGUAGES C CXX)

# Use C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_options(-fembed-bitcode)

# Options
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependencies
add_subdirectory(external/glfw)
add_subdirectory(external/glad)

# Just add it as a raw include as we don't need to compile it lolol
# add_subdirectory(external/glm)


#Setup imgui
file(GLOB IMGUI_SOURCES
    external/imgui/imgui/*.cpp
    external/imgui/imgui/backends/imgui_impl_glfw.cpp
    external/imgui/imgui/backends/imgui_impl_opengl3.cpp
)

add_library(imgui ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC external/imgui/imgui external/imgui/imgui/backends)
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD)
target_link_libraries(imgui glfw)


add_executable(voxel
    src/main.cpp
)

target_include_directories(voxel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glm_tmp
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(voxel PRIVATE
    glfw
    glad
    imgui
)


# On some platforms you may need extra system libs
if (WIN32)
    target_link_libraries(voxel PRIVATE opengl32)
elseif(APPLE)
    find_library(COCOA_LIBRARY Cocoa)
    find_library(IOKIT_LIBRARY IOKit)
    find_library(COREVIDEO_LIBRARY CoreVideo)
    target_link_libraries(voxel PRIVATE
        ${COCOA_LIBRARY}
        ${IOKIT_LIBRARY}
        ${COREVIDEO_LIBRARY}
    )
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(voxel PRIVATE OpenGL::GL)
endif()
