cmake_minimum_required(VERSION 3.20)

# Enable OBJC only on Apple — other platforms don't need it.
if(APPLE)
    project(minivmac VERSION 37.03 LANGUAGES C CXX OBJC OBJCXX)
else()
    project(minivmac VERSION 37.03 LANGUAGES C CXX)
endif()

# ---------------------------------------------------------------------------
# C++17 — all .c files are compiled as C++ via source property below.
# OSGLUCCO.m stays as Objective-C.
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
set(MINIVMAC_BACKEND "auto" CACHE STRING "Platform backend: auto, cocoa, sdl")
set_property(CACHE MINIVMAC_BACKEND PROPERTY STRINGS auto cocoa sdl)

set(MINIVMAC_MAINTAINER "egon.rath@gmail.com" CACHE STRING "Maintainer contact")
set(MINIVMAC_HOMEPAGE "https://github.com/egrath" CACHE STRING "Project homepage")

# Model selection — determines CPU, wire config, VIA layout
set(MINIVMAC_MODEL "II" CACHE STRING "Mac model: II")
set_property(CACHE MINIVMAC_MODEL PROPERTY STRINGS II)

# Display
set(MINIVMAC_SCREEN_WIDTH  "640" CACHE STRING "Screen width in pixels")
set(MINIVMAC_SCREEN_HEIGHT "480" CACHE STRING "Screen height in pixels")
set(MINIVMAC_SCREEN_DEPTH  "3"   CACHE STRING "Screen depth (log2 bits per pixel: 0=1bpp, 3=8bpp)")

# Sound
set(MINIVMAC_SOUND "1" CACHE STRING "Enable sound (1/0)")

# Drives
set(MINIVMAC_NUM_DRIVES "6" CACHE STRING "Number of floppy drives")

# Magnify / fullscreen / speed
set(MINIVMAC_MAGNIFY_ENABLE "1" CACHE STRING "Allow magnification (1/0)")
set(MINIVMAC_MAGNIFY_INIT   "1" CACHE STRING "Start magnified (1/0)")
set(MINIVMAC_WINDOW_SCALE   "2" CACHE STRING "Window scale factor")
set(MINIVMAC_FULLSCREEN_VAR     "1" CACHE STRING "Variable fullscreen (1/0)")
set(MINIVMAC_FULLSCREEN_INIT    "0" CACHE STRING "Start in fullscreen (1/0)")
set(MINIVMAC_FULLSCREEN_MAY     "1" CACHE STRING "Allow fullscreen (1/0)")
set(MINIVMAC_FULLSCREEN_MAY_NOT "1" CACHE STRING "Allow windowed mode (1/0)")
set(MINIVMAC_SPEED "4" CACHE STRING "Initial speed value (0-5, 4=8x)")

# Language
set(MINIVMAC_LANGUAGE "English" CACHE STRING "UI language")
set_property(CACHE MINIVMAC_LANGUAGE PROPERTY STRINGS
    English French German Italian Spanish Dutch PortugueseBrazilian Polish Czech Serbian)

# Debug
set(MINIVMAC_DBGLOG "0" CACHE STRING "Enable debug log (1/0)")
set(MINIVMAC_ABNORMAL_REPORTS "0" CACHE STRING "Enable abnormal reports (1/0)")
set(MINIVMAC_LOCALTALK "0" CACHE STRING "Enable LocalTalk (1/0)")

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
# Model-specific settings (ROM file, checksums, CNFUDPIC.h)
# ---------------------------------------------------------------------------
if(MINIVMAC_MODEL STREQUAL "II")
    set(MINIVMAC_ROM_FILE "MacII.ROM")
    set(MINIVMAC_ROM_SIZE "0x00040000")
    set(MINIVMAC_ROM_CHECKSUM1 "0x9779D2C4")
    set(MINIVMAC_ROM_CHECKSUM2 "0x97221136")
    set(MINIVMAC_CHECK_ROM_CHECKSUM "0")
    set(MINIVMAC_MODEL_CNFUDPIC "${CMAKE_SOURCE_DIR}/cmake/models/MacII_CNFUDPIC.h")
else()
    message(FATAL_ERROR "Unknown model: ${MINIVMAC_MODEL}. Currently supported: II")
endif()

# Compute video RAM size: width * height * bytes_per_pixel, rounded up to
# next power of two. Prevents VRAM overflow at non-standard resolutions.
math(EXPR _pixels "${MINIVMAC_SCREEN_WIDTH} * ${MINIVMAC_SCREEN_HEIGHT}")
math(EXPR _bpp "1 << ${MINIVMAC_SCREEN_DEPTH}")  # depth is log2(bpp)
math(EXPR _vram_bytes "${_pixels} * ${_bpp} / 8")
# Round up to next power of 2
set(_pow2 65536)  # minimum 64KB
while(_pow2 LESS _vram_bytes)
    math(EXPR _pow2 "${_pow2} * 2")
endwhile()
# Format as hex for the C header
math(EXPR _pow2_hex "${_pow2}" OUTPUT_FORMAT HEXADECIMAL)
set(MINIVMAC_VIDMEM_SIZE "${_pow2_hex}")
message(STATUS "Video RAM: ${MINIVMAC_SCREEN_WIDTH}x${MINIVMAC_SCREEN_HEIGHT}@${_bpp}bpp -> ${MINIVMAC_VIDMEM_SIZE}")

# ---------------------------------------------------------------------------
# Language → header mapping
# ---------------------------------------------------------------------------
set(_lang_map_English    "lang/strings_english.h")
set(_lang_map_French     "lang/strings_french.h")
set(_lang_map_German     "lang/strings_german.h")
set(_lang_map_Italian    "lang/strings_italian.h")
set(_lang_map_Spanish    "lang/strings_spanish.h")
set(_lang_map_Dutch      "lang/strings_dutch.h")
set(_lang_map_PortugueseBrazilian "lang/strings_portuguese.h")
set(_lang_map_Polish     "lang/strings_polish.h")
set(_lang_map_Czech      "lang/strings_czech.h")
set(_lang_map_Serbian    "lang/strings_serbian.h")

if(DEFINED _lang_map_${MINIVMAC_LANGUAGE})
    set(MINIVMAC_LANGUAGE_HEADER "${_lang_map_${MINIVMAC_LANGUAGE}}")
else()
    message(FATAL_ERROR "Unknown language: ${MINIVMAC_LANGUAGE}")
endif()

# ---------------------------------------------------------------------------
# Generate remaining config headers
# ---------------------------------------------------------------------------
configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/CNFUDALL.h.in"
    "${MINIVMAC_GENERATED_CFG_DIR}/CNFUDALL.h"
    @ONLY
)
configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/CNFUDOSG.h.in"
    "${MINIVMAC_GENERATED_CFG_DIR}/CNFUDOSG.h"
    @ONLY
)
configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/STRCONST.h.in"
    "${MINIVMAC_GENERATED_CFG_DIR}/STRCONST.h"
    @ONLY
)

# Model-specific CNFUDPIC.h — uses @ONLY to substitute VRAM size
configure_file(
    "${MINIVMAC_MODEL_CNFUDPIC}"
    "${MINIVMAC_GENERATED_CFG_DIR}/CNFUDPIC.h"
    @ONLY
)

# CNFUIALL.h and CNFUIPIC.h are compiler/platform detection — copy as-is
configure_file(
    "${CMAKE_SOURCE_DIR}/src/config/CNFUIALL.h"
    "${MINIVMAC_GENERATED_CFG_DIR}/CNFUIALL.h"
    COPYONLY
)
configure_file(
    "${CMAKE_SOURCE_DIR}/src/config/CNFUIPIC.h"
    "${MINIVMAC_GENERATED_CFG_DIR}/CNFUIPIC.h"
    COPYONLY
)
# Cocoa backend (macOS)
# ---------------------------------------------------------------------------
if(MINIVMAC_BACKEND STREQUAL "cocoa")
    list(APPEND MINIVMAC_SOURCES src/OSGLUCCO.m)

    add_executable(minivmac MACOSX_BUNDLE ${MINIVMAC_SOURCES})

    set_target_properties(minivmac PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/src/config/Info.plist"
        MACOSX_BUNDLE_BUNDLE_NAME "Mini vMac"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.gryphel.minivmac"
    )

    # Bundle resources
    set_source_files_properties(
        src/ICONAPPO.icns
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources
    )
    set_source_files_properties(
        src/config/English.lproj/dummy.txt
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources/English.lproj
    )
    target_sources(minivmac PRIVATE
        src/ICONAPPO.icns
        src/config/English.lproj/dummy.txt
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
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/src/config/Info.plist"
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

# Include paths: generated cfg dir has all 7 config headers (generated or
# copied). Source cfg/ kept as fallback. src/ for source headers.
target_include_directories(minivmac PRIVATE
    "${MINIVMAC_GENERATED_CFG_DIR}"
    "${CMAKE_SOURCE_DIR}/src/config"
    "${CMAKE_SOURCE_DIR}/src"
)

# Compile all .c source files as C++ (do NOT rename files yet — Phase 3).
# OSGLUCCO.m is compiled as Objective-C++ for C++ linkage compatibility.
foreach(_src ${MINIVMAC_SOURCES})
    if(_src MATCHES "\\.c$")
        set_source_files_properties(${_src} PROPERTIES LANGUAGE CXX)
    elseif(_src MATCHES "\\.m$")
        set_source_files_properties(${_src} PROPERTIES LANGUAGE OBJCXX)
    endif()
endforeach()

# Compiler flags matching the original Xcode project
target_compile_options(minivmac PRIVATE
    -Wall
    -Wundef
    -Wno-uninitialized
    -Wno-write-strings
)

# Optimization: Release uses -Os (optimize for size), matching the original
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_C_FLAGS_RELEASE "-Os -DNDEBUG" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-Os -DNDEBUG" CACHE STRING "" FORCE)
set(CMAKE_OBJC_FLAGS_RELEASE "-Os -DNDEBUG" CACHE STRING "" FORCE)
set(CMAKE_OBJCXX_FLAGS_RELEASE "-Os -DNDEBUG" CACHE STRING "" FORCE)
