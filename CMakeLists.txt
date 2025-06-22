cmake_minimum_required(VERSION 3.19)
project(wgvk)
include(FetchContent)

option(WGVK_BUILD_EXAMPLES "Build some WGVK examples" OFF)
option(WGVK_BUILD_WGSL_SUPPORT "Build the WGSL->SPIRV compiler" OFF)
option(WGVK_USE_VMA "Use GPUOpen's VMA allocator (Requires C++)" OFF)


function(wgvk_bundle_libraries output_target)
  function(get_dependencies input_target)
    get_target_property(alias ${input_target} ALIASED_TARGET)
    if(TARGET ${alias})
      set(input_target ${alias})
    endif()
    if(${input_target} IN_LIST all_dependencies)
      return()
    endif()
    list(APPEND all_dependencies ${input_target})
    get_target_property(link_libraries ${input_target} LINK_LIBRARIES)
    foreach(dependency IN LISTS link_libraries)
      if(TARGET ${dependency})
        get_dependencies(${dependency})
      endif()
    endforeach()
    get_target_property(link_libraries ${input_target} INTERFACE_LINK_LIBRARIES)
    foreach(dependency IN LISTS link_libraries)
      if(TARGET ${dependency})
        get_dependencies(${dependency})
      endif()
    endforeach()
    set(all_dependencies ${all_dependencies} PARENT_SCOPE)
  endfunction()
  foreach(input_target IN LISTS ARGN)
    get_dependencies(${input_target})
  endforeach()
  foreach(dependency IN LISTS all_dependencies)
    get_target_property(type ${dependency} TYPE)
    if(${type} STREQUAL "STATIC_LIBRARY")
      list(APPEND all_objects $<TARGET_OBJECTS:${dependency}>)
    elseif(${type} STREQUAL "OBJECT_LIBRARY")
      list(APPEND all_objects $<TARGET_OBJECTS:${dependency}>)
    endif()
  endforeach()

  add_library(${output_target} STATIC ${all_objects})

  add_dependencies(${output_target} ${ARGN})

endfunction()

# Lock in static for now



set(WGVK_CORE_SRC_LIST "src/wgvk.c")
if(SANITIZE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address,undefined")
endif()
if(WGVK_USE_VMA)
    list(APPEND WGVK_CORE_SRC_LIST "src/vma_impl.cpp")
endif()



if(WGVK_BUILD_WGSL_SUPPORT)
    add_library(wgvk_core STATIC ${WGVK_CORE_SRC_LIST})
    target_compile_definitions(wgvk_core PUBLIC RG_STATIC=1 WGVK_DISABLE_ASSERT=1)
    if(WGVK_USE_VMA)
        target_compile_definitions(wgvk_core PUBLIC USE_VMA_ALLOCATOR=1)
    endif()

    target_include_directories(wgvk_core PUBLIC include)

    set(DAWN_ENABLE_DESKTOP_GL OFF)
    set(DAWN_ENABLE_OPENGLES OFF)
    set(DAWN_ENABLE_VULKAN OFF)
    set(DAWN_ENABLE_SPIRV_VALIDATION OFF)
    set(DAWN_BUILD_SAMPLES OFF)
    set(DAWN_USE_GLFW OFF)
    set(DAWN_USE_X11 OFF)
    set(DAWN_USE_WAYLAND OFF)
    set(TINT_BUILD_GLSL_WRITER OFF)
    set(TINT_BUILD_CMD_TOOLS OFF)
    set(TINT_BUILD_SPV_READER OFF)
    set(TINT_BUILD_SPV_WRITER ON)
    set(TINT_BUILD_WGSL_WRITER OFF)
    set(TINT_BUILD_TESTS OFF)
    set(TINT_BUILD_GLSL_VALIDATOR OFF)
    FetchContent_Declare(
        dawn
        URL https://github.com/manuel5975p/dawn_monorepo/archive/refs/tags/v7252.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP True
    )
    FetchContent_MakeAvailable(dawn)
    add_library(tint_c_api STATIC "src/tint_c_api.cpp")
    target_include_directories(tint_c_api PRIVATE "include")
    target_include_directories(tint_c_api PRIVATE "${dawn_SOURCE_DIR}")
    target_compile_features(tint_c_api PRIVATE cxx_std_20)
    target_compile_definitions(wgvk_core PUBLIC SUPPORT_WGSL=1)
    wgvk_bundle_libraries(wgvk wgvk_core tint_lang_wgsl_reader tint_lang_spirv_writer tint_lang_wgsl_inspector tint_c_api)
    target_include_directories(wgvk PUBLIC include)
else()
    add_library(wgvk STATIC ${WGVK_CORE_SRC_LIST})
    target_compile_definitions(wgvk PUBLIC RG_STATIC=1 WGVK_DISABLE_ASSERT=1)
    if(WGVK_USE_VMA)
        target_compile_definitions(wgvk PUBLIC USE_VMA_ALLOCATOR=1)
    endif()

    target_include_directories(wgvk PUBLIC include)

endif()



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

    add_executable(glfw_surface "examples/glfw_surface.c")
    add_executable(basic_compute "examples/basic_compute.c")
    add_executable(rgfw_surface "examples/rgfw_surface.c")
    
    target_link_libraries(glfw_surface PUBLIC wgvk glfw)
    target_link_libraries(basic_compute PUBLIC wgvk glfw)
    target_link_libraries(rgfw_surface PUBLIC wgvk)
endif()