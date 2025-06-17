cmake_minimum_required(VERSION 3.19)
project(wgvk)
include(FetchContent)
option(WGVK_BUILD_EXAMPLES "Build some WGVK examples" ON)

# Lock in static for now
add_library(wgvk STATIC src/wgvk.c)
target_compile_definitions(wgvk PUBLIC RG_STATIC=1)


target_include_directories(wgvk PUBLIC include)


if(WIN32)
    target_compile_definitions(wgvk PUBLIC SUPPORT_WIN32_SURFACE=1)
# For Apple (macOS, iOS) using Metal
elseif(APPLE)
    target_compile_definitions(wgvk PUBLIC SUPPORT_METAL_SURFACE=1)
    # Link against frameworks required for a CAMetalLayer surface
    target_link_libraries(wgvk PUBLIC "-framework Cocoa" "-framework Metal" "-framework QuartzCore")
# For other Unix-like systems (primarily Linux)
elseif(UNIX)
    find_package(PkgConfig QUIET)
        
    # Wayland Check
    pkg_check_modules(WAYLAND_CLIENT QUIET wayland-client)
    if(WAYLAND_CLIENT_FOUND)
        message(STATUS "Wayland found")
        target_compile_definitions(wgvk PUBLIC SUPPORT_WAYLAND_SURFACE=1)
        set(GLFW_BUILD_WAYLAND ON)
        target_link_libraries(wgvk PUBLIC wayland-client)
    endif()

    # Check for X11 (Xlib) support
    find_package(X11 QUIET)
    if(X11_FOUND)
        set(GLFW_BUILD_X11 ON)
        target_compile_definitions(wgvk PUBLIC SUPPORT_XLIB_SURFACE=1)
        target_link_libraries(wgvk PUBLIC X11::X11)
    endif()
endif()

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