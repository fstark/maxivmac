cmake_minimum_required(VERSION 3.20)

project(maxivmac VERSION 37.03 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# C++17
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Version from git
# ---------------------------------------------------------------------------
execute_process(
    COMMAND git describe --tags --match "v*" --always
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE GIT_RESULT
)
if(NOT GIT_RESULT EQUAL 0 OR GIT_VERSION STREQUAL "")
    set(GIT_VERSION "dev-unknown")
elseif(NOT GIT_VERSION MATCHES "^v")
    set(GIT_VERSION "dev-${GIT_VERSION}")
endif()
message(STATUS "Version: ${GIT_VERSION}")

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------

# Debug
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
    src/core/state_recorder.cpp
    src/devices/device.cpp
    src/cpu/m68k_tables.cpp
    src/cpu/m68k.cpp
    src/cpu/cpu.cpp
    src/devices/via_base.cpp
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
    src/platform/common/mac_roman.cpp
    src/platform/common/path_utils.cpp
    src/platform/common/dbglog_platform.cpp
    src/platform/common/disk_io.cpp
    src/platform/common/rom_loader.cpp
    src/lang/localization.cpp
    src/lang/lang_english.cpp
    src/lang/lang_french.cpp
    src/lang/lang_german.cpp
    src/lang/lang_italian.cpp
    src/lang/lang_spanish.cpp
    src/lang/lang_dutch.cpp
    src/lang/lang_portuguese.cpp
    src/lang/lang_polish.cpp
    src/lang/lang_czech.cpp
    src/lang/lang_serbian.cpp
    src/lang/lang_catalan.cpp
)

# ---------------------------------------------------------------------------
# SDL target (Linux / Windows / macOS)
# ---------------------------------------------------------------------------
list(APPEND MINIVMAC_SOURCES src/platform/sdl.cpp)

add_executable(maxivmac ${MINIVMAC_SOURCES})

# Try SDL3
find_package(SDL3 REQUIRED)
message(STATUS "Found SDL3")
target_link_libraries(maxivmac PRIVATE SDL3::SDL3)

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

# ---------------------------------------------------------------------------
# Common settings
# ---------------------------------------------------------------------------

# Include paths: src/ for source headers.
target_include_directories(maxivmac PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
)

# Build-time definitions: version string and optional feature toggles
target_compile_definitions(maxivmac PRIVATE
    "MAXIVMAC_VERSION=\"${GIT_VERSION}\""
    "WantAbnormalReports=${MINIVMAC_ABNORMAL_REPORTS}"
    "EmLocalTalk=${MINIVMAC_LOCALTALK}"
    "dbglog_HAVE=1"
)

# Compiler flags matching the original Xcode project
target_compile_options(maxivmac PRIVATE
    -Wall
    -Wextra
    -Werror
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
