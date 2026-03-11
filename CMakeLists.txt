cmake_minimum_required(VERSION 3.20)

# Enable OBJC only on Apple — other platforms don't need it.
if(APPLE)
    project(minivmac VERSION 37.03 LANGUAGES C OBJC)
else()
    project(minivmac VERSION 37.03 LANGUAGES C)
endif()

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
set(MINIVMAC_BACKEND "auto" CACHE STRING "Platform backend: auto, cocoa, sdl")
set_property(CACHE MINIVMAC_BACKEND PROPERTY STRINGS auto cocoa sdl)

set(MINIVMAC_MAINTAINER "egon.rath@gmail.com" CACHE STRING "Maintainer contact")
set(MINIVMAC_HOMEPAGE "https://github.com/egrath" CACHE STRING "Project homepage")

# ---------------------------------------------------------------------------
# Source files — all compilation units from the project.
# Files guarded by config #defines compile to empty translation units when
# their feature is disabled, so it's safe to always include them.
# ---------------------------------------------------------------------------
set(MINIVMAC_SOURCES
    src/GLOBGLUE.c
    src/M68KITAB.c
    src/MINEM68K.c
    src/VIAEMDEV.c
    src/VIA2EMDV.c
    src/IWMEMDEV.c
    src/SCCEMDEV.c
    src/RTCEMDEV.c
    src/ROMEMDEV.c
    src/SCSIEMDV.c
    src/SONYEMDV.c
    src/SCRNEMDV.c
    src/VIDEMDEV.c
    src/MOUSEMDV.c
    src/ADBEMDEV.c
    src/ASCEMDEV.c
    src/SNDEMDEV.c
    src/KBRDEMDV.c
    src/PMUEMDEV.c
    src/DISAM68K.c
    src/PROGMAIN.c
)

# ---------------------------------------------------------------------------
# Resolve "auto" backend
# ---------------------------------------------------------------------------
if(MINIVMAC_BACKEND STREQUAL "auto")
    if(APPLE)
        set(MINIVMAC_BACKEND "cocoa")
    else()
        set(MINIVMAC_BACKEND "sdl")
    endif()
endif()

message(STATUS "minivmac backend: ${MINIVMAC_BACKEND}")

# ---------------------------------------------------------------------------
# Generate CNFUIOSG.h from template — selects backend + system includes
# ---------------------------------------------------------------------------
# The generated header lands in the build tree and is found before cfg/
set(MINIVMAC_GENERATED_CFG_DIR "${CMAKE_BINARY_DIR}/cfg_generated")
file(MAKE_DIRECTORY "${MINIVMAC_GENERATED_CFG_DIR}")

if(MINIVMAC_BACKEND STREQUAL "cocoa")
    set(MINIVMAC_BACKEND_TAG "CCO")
    set(MINIVMAC_IS_BUNDLE 1)
    set(CNFUIOSG_SYSTEM_INCLUDES
"#import <Cocoa/Cocoa.h>
#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>
#include <OpenGL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/time.h>")
elseif(MINIVMAC_BACKEND STREQUAL "sdl")
    set(MINIVMAC_BACKEND_TAG "SDL")
    if(APPLE)
        set(MINIVMAC_IS_BUNDLE 1)
    else()
        set(MINIVMAC_IS_BUNDLE 0)
    endif()
    set(CNFUIOSG_SYSTEM_INCLUDES
"#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/time.h>")
else()
    message(FATAL_ERROR "Unknown backend: ${MINIVMAC_BACKEND}. Use cocoa or sdl.")
endif()

configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/CNFUIOSG.h.in"
    "${MINIVMAC_GENERATED_CFG_DIR}/CNFUIOSG.h"
    @ONLY
)

# ---------------------------------------------------------------------------
# Cocoa backend (macOS)
# ---------------------------------------------------------------------------
if(MINIVMAC_BACKEND STREQUAL "cocoa")
    list(APPEND MINIVMAC_SOURCES src/OSGLUCCO.m)

    add_executable(minivmac MACOSX_BUNDLE ${MINIVMAC_SOURCES})

    set_target_properties(minivmac PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/cfg/Info.plist"
        MACOSX_BUNDLE_BUNDLE_NAME "Mini vMac"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.gryphel.minivmac"
    )

    # Bundle resources
    set_source_files_properties(
        src/ICONAPPO.icns
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources
    )
    set_source_files_properties(
        cfg/English.lproj/dummy.txt
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources/English.lproj
    )
    target_sources(minivmac PRIVATE
        src/ICONAPPO.icns
        cfg/English.lproj/dummy.txt
    )

    # macOS frameworks
    find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
    find_library(AUDIOUNIT_FRAMEWORK AudioUnit REQUIRED)
    find_library(COREAUDIO_FRAMEWORK CoreAudio REQUIRED)
    find_library(OPENGL_FRAMEWORK OpenGL REQUIRED)
    target_link_libraries(minivmac PRIVATE
        ${APPKIT_FRAMEWORK}
        ${AUDIOUNIT_FRAMEWORK}
        ${COREAUDIO_FRAMEWORK}
        ${OPENGL_FRAMEWORK}
    )

    # Deployment target
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "macOS deployment target")

# ---------------------------------------------------------------------------
# SDL backend (Linux / Windows / macOS-SDL)
# ---------------------------------------------------------------------------
elseif(MINIVMAC_BACKEND STREQUAL "sdl")
    list(APPEND MINIVMAC_SOURCES src/OSGLUSDL.c)

    if(APPLE)
        add_executable(minivmac MACOSX_BUNDLE ${MINIVMAC_SOURCES})
        set_target_properties(minivmac PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/cfg/Info.plist"
            MACOSX_BUNDLE_BUNDLE_NAME "Mini vMac"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.gryphel.minivmac"
        )
    else()
        add_executable(minivmac ${MINIVMAC_SOURCES})
    endif()

    # Try SDL3 first, fall back to SDL2
    find_package(SDL3 QUIET)
    if(SDL3_FOUND)
        message(STATUS "Found SDL3")
        target_link_libraries(minivmac PRIVATE SDL3::SDL3)
    else()
        find_package(SDL2 REQUIRED)
        message(STATUS "Found SDL2")
        target_link_libraries(minivmac PRIVATE SDL2::SDL2)
    endif()

    # On Linux, we may need X11
    if(UNIX AND NOT APPLE)
        find_package(X11)
        if(X11_FOUND)
            target_link_libraries(minivmac PRIVATE X11::X11)
        endif()
    endif()

    # On Windows, link winsock etc. if needed
    if(WIN32)
        target_link_libraries(minivmac PRIVATE winmm)
    endif()
endif()

# ---------------------------------------------------------------------------
# Common settings
# ---------------------------------------------------------------------------

# Include paths: generated cfg first (so generated CNFUIOSG.h wins), then
# source cfg/, then src/ for source headers.
target_include_directories(minivmac PRIVATE
    "${MINIVMAC_GENERATED_CFG_DIR}"
    "${CMAKE_SOURCE_DIR}/cfg"
    "${CMAKE_SOURCE_DIR}/src"
)

# Compiler flags matching the original Xcode project
target_compile_options(minivmac PRIVATE
    -Wall
    -Wundef
    -Wno-uninitialized
)

# Optimization: Release uses -Os (optimize for size), matching the original
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_C_FLAGS_RELEASE "-Os -DNDEBUG" CACHE STRING "" FORCE)
set(CMAKE_OBJC_FLAGS_RELEASE "-Os -DNDEBUG" CACHE STRING "" FORCE)
