cmake_minimum_required(VERSION 3.19)
project(wgvk)
include(FetchContent)
option(WGVK_BUILD_EXAMPLES "Build some WGVK examples" ON)

# Lock in static for now
add_library(wgvk STATIC src/wgvk.c)
target_compile_definitions(wgvk PUBLIC RG_STATIC=1)


target_include_directories(wgvk PUBLIC include)
target_compile_definitions(wgvk PUBLIC SUPPORT_XLIB_SURFACE=1)


if(WGVK_BUILD_EXAMPLES)
    FetchContent_Declare(
        glfw
        URL https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP True
    )
    FetchContent_MakeAvailable(glfw)

    add_executable(surface_wgvk "examples/surface_wgvk.c")
    
    target_link_libraries(surface_wgvk PUBLIC wgvk glfw)
endif()