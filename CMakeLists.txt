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

# Debug
set(MINIVMAC_ABNORMAL_REPORTS "0" CACHE STRING "Enable abnormal reports (1/0)")
set(MINIVMAC_LOCALTALK "0" CACHE STRING "Enable LocalTalk (1/0)")

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------
set(MINIVMAC_SOURCES
    # Core
    src/core/machine_config.cpp
    src/core/model_defs.cpp
    src/core/config_loader.cpp
    src/config/mac_file.cpp
    src/core/wire_bus.cpp
    src/core/ict_scheduler.cpp
    src/core/rig.cpp
    src/core/machine.cpp
    src/core/extn_clip.cpp
    src/core/extn_extfs.cpp
    src/core/extfs_log.cpp
    src/core/diag.cpp
    src/core/state_recorder.cpp

    # Utility
    src/util/macroman.cpp

    # Storage
    src/storage/appledouble.cpp
    src/storage/filename_encoding.cpp
    src/storage/text_convert.cpp
    src/storage/host_volume.cpp
    src/storage/drive_manager.cpp

    # Language / type system
    src/lang/type_registry.cpp
    src/lang/global_registry.cpp

    # CPU
    src/cpu/m68k_tables.cpp
    src/cpu/m68k.cpp
    src/cpu/trap_counter.cpp
    src/cpu/trap_defs.cpp
    src/cpu/trap_tracer.cpp
    src/cpu/cpu.cpp
    src/cpu/disasm.cpp

    # Debugger
    src/debugger/debugger.cpp
    src/debugger/dbg_io.cpp
    src/debugger/dbg_client.cpp
    src/debugger/cmd_parser.cpp
    src/debugger/cmd_exec.cpp
    src/debugger/cmd_break.cpp
    src/debugger/cmd_memory.cpp
    src/debugger/cmd_trace.cpp
    src/debugger/cmd_diag.cpp
    src/debugger/cmd_info.cpp
    src/debugger/cmd_help.cpp
    src/debugger/cmd_drive.cpp
    src/debugger/symbols.cpp
    src/debugger/expr.cpp
    src/debugger/script_suspend.cpp

    # Devices
    src/devices/device.cpp
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

    # Platform — common
    src/core/main.cpp
    src/platform/common/osglu_common.cpp
    src/platform/common/event_queue.cpp
    src/platform/common/param_buffers.cpp
    src/platform/common/keyboard_map.cpp
    src/platform/common/path_utils.cpp
    src/platform/common/dbglog_platform.cpp
    src/platform/common/tick_timer.cpp
    src/platform/common/disk_io.cpp
    src/platform/common/clipboard.cpp
    src/platform/common/rom_loader.cpp
    src/platform/screen_convert.cpp
    src/platform/emulator_shell.cpp

    # Platform — SDL (windowing, audio, keyboard)
    src/platform/sdl_sound.cpp
    src/platform/sdl_keyboard.cpp

    # Platform — ImGui backend
    src/platform/imgui_backend.cpp
    src/platform/imgui_launcher.cpp
    src/platform/imgui_overlay.cpp
    src/platform/lomem_globals.cpp
    src/platform/clipboard_image.cpp
    src/platform/stb_impl.cpp

    # Platform — Headless backend
    src/platform/headless_backend.cpp

    # Unified entry point
    src/platform/app_main.cpp
)

add_executable(maxivmac ${MINIVMAC_SOURCES})

# ---------------------------------------------------------------------------
# SDL3 (always required — headless mode uses null stubs at runtime)
# ---------------------------------------------------------------------------
find_package(SDL3 REQUIRED)
message(STATUS "Found SDL3")
target_link_libraries(maxivmac PRIVATE SDL3::SDL3)

# ---------------------------------------------------------------------------
# ImGui
# ---------------------------------------------------------------------------
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
    "${CMAKE_SOURCE_DIR}/libs/stb"
)

# Build-time definitions: version string and optional feature toggles
target_compile_definitions(maxivmac PRIVATE
    "MAXIVMAC_VERSION=\"${GIT_VERSION}\""
    "WantAbnormalReports=${MINIVMAC_ABNORMAL_REPORTS}"
    "EmLocalTalk=${MINIVMAC_LOCALTALK}"
    "HAVE_SDL=1"
)

# Compiler flags matching the original Xcode project
target_compile_options(maxivmac PRIVATE
    -Wall
    -Wextra
    -Werror
    -Wundef
    -Wno-write-strings
)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(maxivmac PRIVATE -Wconditional-uninitialized)
endif()
if(MINGW)
    target_compile_definitions(maxivmac PRIVATE __USE_MINGW_ANSI_STDIO=1)
endif()

# Third-party code: suppress warnings-as-errors
set_source_files_properties(src/platform/stb_impl.cpp PROPERTIES
    COMPILE_FLAGS "-Wno-error"
)
set_source_files_properties(${IMGUI_SOURCES} PROPERTIES
    COMPILE_FLAGS "-Wno-error"
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
# Icon extraction tool
# ---------------------------------------------------------------------------
add_executable(icon-extract
    tools/icon_extract/icon_extract.cpp
    tools/icon_extract/resource_fork.cpp
    tools/icon_extract/icon_decode.cpp
    tools/icon_extract/icns_decode.cpp
    tools/icon_extract/png_text.cpp
    tools/icon_extract/stb_impl.cpp
    src/storage/appledouble.cpp
    src/storage/filename_encoding.cpp
    src/storage/text_convert.cpp
    src/util/macroman.cpp
)
target_include_directories(icon-extract PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/tools/icon_extract"
    "${CMAKE_SOURCE_DIR}/libs/stb"
    "${CMAKE_SOURCE_DIR}/libs"
)
target_compile_options(icon-extract PRIVATE -Wall -Wextra -Werror)
set_source_files_properties(tools/icon_extract/stb_impl.cpp PROPERTIES
    COMPILE_FLAGS "-Wno-error"
)

# ---------------------------------------------------------------------------
# Unit tests
# ---------------------------------------------------------------------------
enable_testing()
add_executable(tests
    test/test_main.cpp
    test/test_slip.cpp
    test/test_tracing.cpp
    test/test_debugger.cpp
    test/test_appledouble.cpp
    test/test_host_volume.cpp
    test/test_stubs.cpp
    test/test_resource_fork.cpp
    test/test_event_queue.cpp
    src/platform/common/event_queue.cpp
    src/cpu/trap_defs.cpp
    src/cpu/trap_tracer.cpp
    src/cpu/trap_counter.cpp
    src/platform/lomem_globals.cpp
    src/debugger/symbols.cpp
    src/debugger/expr.cpp
    src/debugger/cmd_parser.cpp
    src/debugger/dbg_io.cpp
    src/storage/appledouble.cpp
    src/storage/filename_encoding.cpp
    src/storage/text_convert.cpp
    src/storage/host_volume.cpp
    src/storage/drive_manager.cpp
    src/core/diag.cpp
    src/lang/type_registry.cpp
    src/lang/global_registry.cpp
    src/util/macroman.cpp
    tools/icon_extract/resource_fork.cpp
    tools/icon_extract/icon_decode.cpp
    test/test_drive_manager.cpp
    test/test_types.cpp
    test/test_ui.cpp
    test/test_model_defs.cpp
    test/test_mac_file.cpp
    src/core/model_defs.cpp
    src/core/machine_config.cpp
    src/core/config_loader.cpp
    src/config/mac_file.cpp
)
target_include_directories(tests PRIVATE "${CMAKE_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/libs" "${CMAKE_SOURCE_DIR}/tools/icon_extract")
target_compile_options(tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME tests COMMAND tests WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
