cmake_minimum_required(VERSION 3.20)

# Enable OBJC only on Apple — other platforms don't need it.
if(APPLE)
    project(maxivmac VERSION 37.03 LANGUAGES C CXX OBJC OBJCXX)
else()
    project(maxivmac VERSION 37.03 LANGUAGES C CXX)
endif()

# ---------------------------------------------------------------------------
# C++17 — all sources are .cpp or .mm (ObjC++).
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

# Model selection — now runtime via --model flag.
# MINIVMAC_MODEL is retained only as a hint for CNFUDOSG.h defaults.

# Screen dimensions are now runtime — see MachineConfig.

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
set(MINIVMAC_DBGLOG "1" CACHE STRING "Enable debug log (1/0)")
set(MINIVMAC_ABNORMAL_REPORTS "0" CACHE STRING "Enable abnormal reports (1/0)")
set(MINIVMAC_LOCALTALK "0" CACHE STRING "Enable LocalTalk (1/0)")

# ---------------------------------------------------------------------------
# Source files — all compilation units from the project.
# Files guarded by config #defines compile to empty translation units when
# their feature is disabled, so it's safe to always include them.
# ---------------------------------------------------------------------------
set(MINIVMAC_SOURCES
    src/core/machine_config.cpp
    src/core/config_loader.cpp
    src/core/wire_bus.cpp
    src/core/ict_scheduler.cpp
    src/core/machine_obj.cpp
    src/core/machine.cpp
    src/devices/device.cpp
    src/cpu/m68k_tables.cpp
    src/cpu/m68k.cpp
    src/cpu/cpu.cpp
    src/devices/via.cpp
    src/devices/via2.cpp
    src/devices/iwm.cpp
    src/devices/scc.cpp
    src/devices/rtc.cpp
    src/devices/rom.cpp
    src/devices/scsi.cpp
    src/devices/sony.cpp
    src/devices/screen.cpp
    src/devices/video.cpp
    src/devices/mouse.cpp
    src/devices/adb.cpp
    src/devices/asc.cpp
    src/devices/sound.cpp
    src/devices/keyboard.cpp
    src/devices/pmu.cpp
    src/cpu/disasm.cpp
    src/core/main.cpp
    src/platform/common/osglu_common.cpp
    src/platform/common/intl_chars.cpp
    src/platform/common/param_buffers.cpp
    src/platform/common/control_mode.cpp
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

message(STATUS "maxivmac backend: ${MINIVMAC_BACKEND}")

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
# ROM / model defaults for CNFUDOSG.h template (compile-time fallbacks).
# Actual ROM file, size, and base address are runtime — see MachineConfig.
# ---------------------------------------------------------------------------
set(MINIVMAC_ROM_FILE "MacII.ROM")
set(MINIVMAC_ROM_CHECKSUM1 "0x9779D2C4")
set(MINIVMAC_ROM_CHECKSUM2 "0x97221136")
set(MINIVMAC_CHECK_ROM_CHECKSUM "0")

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

# CNFUDPIC.h — now model-independent (all model-specific config is runtime).
configure_file(
    "${CMAKE_SOURCE_DIR}/src/config/CNFUDPIC.h"
    "${MINIVMAC_GENERATED_CFG_DIR}/CNFUDPIC.h"
    COPYONLY
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
    list(APPEND MINIVMAC_SOURCES src/platform/cocoa.mm)

    # osglu_common.cpp and intl_chars.cpp include headers that pull in Cocoa.h (ObjC)
    set_source_files_properties(
        src/platform/common/osglu_common.cpp
        src/platform/common/intl_chars.cpp
        src/platform/common/param_buffers.cpp
        src/platform/common/control_mode.cpp
        PROPERTIES LANGUAGE OBJCXX)

    add_executable(maxivmac MACOSX_BUNDLE ${MINIVMAC_SOURCES})

    set_target_properties(maxivmac PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/src/config/Info.plist"
        MACOSX_BUNDLE_BUNDLE_NAME "Maxi vMac"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.gryphel.maxivmac"
    )

    # Bundle resources
    set_source_files_properties(
        src/resources/ICONAPPO.icns
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources
    )
    set_source_files_properties(
        src/config/English.lproj/dummy.txt
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources/English.lproj
    )
    target_sources(maxivmac PRIVATE
        src/resources/ICONAPPO.icns
        src/config/English.lproj/dummy.txt
    )

    # macOS frameworks
    find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
    find_library(AUDIOUNIT_FRAMEWORK AudioUnit REQUIRED)
    find_library(COREAUDIO_FRAMEWORK CoreAudio REQUIRED)
    find_library(OPENGL_FRAMEWORK OpenGL REQUIRED)
    target_link_libraries(maxivmac PRIVATE
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
    list(APPEND MINIVMAC_SOURCES src/platform/sdl.cpp)

    if(APPLE)
        add_executable(maxivmac MACOSX_BUNDLE ${MINIVMAC_SOURCES})
        set_target_properties(maxivmac PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/src/config/Info.plist"
            MACOSX_BUNDLE_BUNDLE_NAME "Maxi vMac"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.gryphel.maxivmac"
        )
    else()
        add_executable(maxivmac ${MINIVMAC_SOURCES})
    endif()

    # Try SDL3 first, fall back to SDL2
    find_package(SDL3 QUIET)
    if(SDL3_FOUND)
        message(STATUS "Found SDL3")
        target_link_libraries(maxivmac PRIVATE SDL3::SDL3)
    else()
        find_package(SDL2 REQUIRED)
        message(STATUS "Found SDL2")
        target_link_libraries(maxivmac PRIVATE SDL2::SDL2)
    endif()

    # On Linux, we may need X11
    if(UNIX AND NOT APPLE)
        find_package(X11)
        if(X11_FOUND)
            target_link_libraries(maxivmac PRIVATE X11::X11)
        endif()
    endif()

    # On Windows, link winsock etc. if needed
    if(WIN32)
        target_link_libraries(maxivmac PRIVATE winmm)
    endif()
endif()

# ---------------------------------------------------------------------------
# Common settings
# ---------------------------------------------------------------------------

# Include paths: generated cfg dir has all 7 config headers (generated or
# copied). src/config/ for static fallback configs. src/ for source headers.
target_include_directories(maxivmac PRIVATE
    "${MINIVMAC_GENERATED_CFG_DIR}"
    "${CMAKE_SOURCE_DIR}/src/config"
    "${CMAKE_SOURCE_DIR}/src"
)

# All sources are now .cpp or .mm — no LANGUAGE overrides needed.

# Compiler flags matching the original Xcode project
target_compile_options(maxivmac PRIVATE
    -Wall
    -Wundef
    -Wconditional-uninitialized
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
