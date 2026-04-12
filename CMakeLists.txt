cmake_minimum_required(VERSION 3.20)

project(maxivmac VERSION 37.03 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# C++23
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
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

# Backend selection: sdl (default), headless, imgui
set(MAXIVMAC_BACKEND "sdl" CACHE STRING "Backend: sdl, imgui, headless")

# Debug
set(MINIVMAC_ABNORMAL_REPORTS "0" CACHE STRING "Enable abnormal reports (1/0)")
set(MINIVMAC_LOCALTALK "0" CACHE STRING "Enable LocalTalk (1/0)")

# ---------------------------------------------------------------------------
# Common source files — no SDL dependency.
# These compile for all backends (sdl, imgui, headless).
# ---------------------------------------------------------------------------
set(COMMON_SOURCES
    src/core/machine_config.cpp
    src/core/config_loader.cpp
    src/core/wire_bus.cpp
    src/core/ict_scheduler.cpp
    src/core/machine_obj.cpp
    src/core/machine.cpp
    src/core/extn_clip.cpp
    src/core/state_recorder.cpp
    src/devices/device.cpp
    src/cpu/m68k_tables.cpp
    src/cpu/m68k.cpp
    src/cpu/trap_counter.cpp
    src/cpu/cpu.cpp
    src/devices/via_base.cpp
    src/devices/via.cpp
    src/devices/via2.cpp
    src/devices/iwm.cpp
    src/devices/scc.cpp
    src/devices/serial_factory.cpp
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
    src/platform/common/param_buffers.cpp
    src/platform/common/keyboard_map.cpp
    src/platform/common/mac_roman.cpp
    src/platform/common/path_utils.cpp
    src/platform/common/dbglog_platform.cpp
    src/platform/common/tick_timer.cpp
    src/platform/common/disk_io.cpp
    src/platform/common/clipboard.cpp
    src/platform/common/rom_loader.cpp
    src/platform/screen_convert.cpp
    src/platform/emulator_shell.cpp
)

# ---------------------------------------------------------------------------
# SDL-common sources — shared by imgui backend (SDL3 for windowing/audio).
# Not compiled for headless builds.
# ---------------------------------------------------------------------------
set(SDL_COMMON_SOURCES
    src/platform/sdl_sound.cpp
    src/platform/sdl_keyboard.cpp
)

# ---------------------------------------------------------------------------
# Backend-specific sources
# ---------------------------------------------------------------------------
if(MAXIVMAC_BACKEND STREQUAL "imgui")
    set(BACKEND_SOURCES
        src/platform/imgui_backend.cpp
        src/platform/imgui_debug_windows.cpp
        src/platform/imgui_model_selector.cpp
        src/platform/imgui_overlay.cpp
        src/platform/imgui_tool_registry.cpp
        src/platform/imgui_lomem_tool.cpp
        src/platform/lomem_globals.cpp
        src/platform/imgui_main.cpp
    )
    set(NEED_SDL ON)
    set(NEED_IMGUI ON)
elseif(MAXIVMAC_BACKEND STREQUAL "headless")
    set(BACKEND_SOURCES
        src/platform/headless_backend.cpp
        src/platform/headless_main.cpp
        src/platform/null_sound.cpp
        src/platform/null_keyboard.cpp
    )
    set(NEED_SDL OFF)
else()
    message(FATAL_ERROR "Unknown backend: ${MAXIVMAC_BACKEND}")
endif()

message(STATUS "Backend: ${MAXIVMAC_BACKEND}")

# ---------------------------------------------------------------------------
# Assemble final source list
# ---------------------------------------------------------------------------
set(MINIVMAC_SOURCES ${COMMON_SOURCES} ${BACKEND_SOURCES})
if(NEED_SDL)
    list(APPEND MINIVMAC_SOURCES ${SDL_COMMON_SOURCES})
endif()

add_executable(maxivmac ${MINIVMAC_SOURCES})

# ---------------------------------------------------------------------------
# SDL (required for sdl and imgui backends)
# ---------------------------------------------------------------------------
if(NEED_SDL)
    find_package(SDL3 REQUIRED)
    message(STATUS "Found SDL3")
    target_link_libraries(maxivmac PRIVATE SDL3::SDL3)
endif()

# ---------------------------------------------------------------------------
# ImGui (required for imgui backend)
# ---------------------------------------------------------------------------
if(NEED_IMGUI)
    set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/libs/imgui")
    if(NOT EXISTS "${IMGUI_DIR}/imgui.h")
        message(FATAL_ERROR "ImGui not found at ${IMGUI_DIR}. "
            "Please vendor Dear ImGui into libs/imgui/.")
    endif()
    set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_sources(maxivmac PRIVATE ${IMGUI_SOURCES})
    target_include_directories(maxivmac PRIVATE
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
    )
    find_package(OpenGL REQUIRED)
    target_link_libraries(maxivmac PRIVATE OpenGL::GL)
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
)

if(NEED_SDL)
    target_compile_definitions(maxivmac PRIVATE "HAVE_SDL=1")
endif()

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

# ---------------------------------------------------------------------------
# Code coverage (clang source-based)
# ---------------------------------------------------------------------------
option(MAXIVMAC_COVERAGE "Build with code coverage instrumentation" OFF)
if(MAXIVMAC_COVERAGE)
    target_compile_options(maxivmac PRIVATE
        -fprofile-instr-generate -fcoverage-mapping)
    target_link_options(maxivmac PRIVATE
        -fprofile-instr-generate -fcoverage-mapping)
endif()

# ---------------------------------------------------------------------------
# Sanitizers (ASan + UBSan)
# ---------------------------------------------------------------------------
option(MAXIVMAC_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(MAXIVMAC_SANITIZE)
    target_compile_options(maxivmac PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(maxivmac PRIVATE
        -fsanitize=address,undefined)
endif()

# ---------------------------------------------------------------------------
# Networking (auto-detected: uses libslirp when available)
# ---------------------------------------------------------------------------
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SLIRP IMPORTED_TARGET slirp)
endif()
if(SLIRP_FOUND)
    target_sources(maxivmac PRIVATE src/devices/serial_slirp.cpp)
    target_link_libraries(maxivmac PRIVATE PkgConfig::SLIRP)
    target_compile_definitions(maxivmac PRIVATE "HAVE_SLIRP=1")
    message(STATUS "Networking: enabled (libslirp ${SLIRP_VERSION})")
else()
    target_compile_definitions(maxivmac PRIVATE "HAVE_SLIRP=0")
    message(STATUS "Networking: disabled (libslirp not found)")
endif()

# ---------------------------------------------------------------------------
# Unit tests
# ---------------------------------------------------------------------------
add_executable(test_slip test/test_slip.cpp)
target_include_directories(test_slip PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_compile_options(test_slip PRIVATE -Wall -Wextra -Werror -UNDEBUG)
