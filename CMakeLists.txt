cmake_minimum_required(VERSION 3.20)
project(minivmac VERSION 37.03 LANGUAGES C OBJC)

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
# Platform backend
# ---------------------------------------------------------------------------
set(MINIVMAC_BACKEND "auto" CACHE STRING "Platform backend: auto, cocoa, sdl")
set_property(CACHE MINIVMAC_BACKEND PROPERTY STRINGS auto cocoa sdl)

if(MINIVMAC_BACKEND STREQUAL "auto")
    if(APPLE)
        set(MINIVMAC_BACKEND "cocoa")
    else()
        set(MINIVMAC_BACKEND "sdl")
    endif()
endif()

message(STATUS "minivmac backend: ${MINIVMAC_BACKEND}")

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

    add_executable(minivmac ${MINIVMAC_SOURCES})

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

    # On Linux, we may need X11 and ALSA
    if(UNIX AND NOT APPLE)
        find_package(X11)
        if(X11_FOUND)
            target_link_libraries(minivmac PRIVATE X11::X11)
        endif()
    endif()

else()
    message(FATAL_ERROR "Unknown backend: ${MINIVMAC_BACKEND}. Use cocoa or sdl.")
endif()

# ---------------------------------------------------------------------------
# Common settings
# ---------------------------------------------------------------------------
target_include_directories(minivmac PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/cfg"
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
