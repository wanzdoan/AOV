cmake_minimum_required(VERSION 3.22.1)
project(aov_zygisk)

# =============================================================================
#  AOV Zygisk — CMakeLists.txt
#  Build system for the AOV (Liên Quân Mobile) Zygisk mod module
#  Target ABI: arm64-v8a (AOV only ships arm64)
# =============================================================================

# ── Language Standard ─────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Compiler Flags ────────────────────────────────────────────────────────────
# LTO: reduces binary size & improves performance
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

# Hide all symbols by default (reduces attack surface for AC scans)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

add_compile_options(
    -O3                         # Full optimization
    -fvisibility=hidden         # Hide symbols
    -ffunction-sections         # Dead code elimination per function
    -fdata-sections             # Dead code elimination per data
    -fstack-protector-strong    # Stack overflow protection
    -Wno-unused-parameter       # Suppress unused param warnings
    -Wno-unused-variable
    -DANDROID                   # Android target flag
)

option(AOV_KEEP_SYMBOLS "Keep symbols in a diagnostic build" OFF)
add_link_options(-Wl,--gc-sections)
if(NOT AOV_KEEP_SYMBOLS)
    add_link_options(-Wl,--strip-all)
endif()

# ── OpenGL ES3 ────────────────────────────────────────────────────────────────
# Stock imgui_impl_opengl3.cpp targets desktop GL by default.
# IMGUI_IMPL_OPENGL_ES3 makes it #include <GLES3/gl3.h> directly (no loader),
# which is what an Android game process provides.
add_compile_definitions(IMGUI_IMPL_OPENGL_ES3)

# ── Include Paths ─────────────────────────────────────────────────────────────
include_directories(
    ${CMAKE_SOURCE_DIR}                        # for #include "include/..."
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/vendor/zygisk/include
    ${CMAKE_SOURCE_DIR}/vendor/dobby/include
    ${CMAKE_SOURCE_DIR}/vendor/imgui
    ${CMAKE_SOURCE_DIR}/vendor/imgui/backends
)

# ── ImGui Sources ─────────────────────────────────────────────────────────────
# Download Dear ImGui from: https://github.com/ocornut/imgui
# Place files in: jni/vendor/imgui/
# Required files:
#   imgui.cpp, imgui_draw.cpp, imgui_tables.cpp, imgui_widgets.cpp
#   backends/imgui_impl_opengl3.cpp
#   backends/imgui_impl_android.cpp  (from ALEX5402 or imgui docking branch)
set(IMGUI_SOURCES
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/backends/imgui_impl_opengl3.cpp
    vendor/imgui/backends/imgui_impl_android.cpp
)

# ── Module Sources ────────────────────────────────────────────────────────────
set(AOVMOD_SOURCES
    # Entry point
    main.cpp

    # Diagnostics (logcat only, finds real touch entry point)
    src/diag/diag.cpp

    # Global state
    src/globals.cpp

    # ── IL2Cpp by-name resolver (ARM64 BL-histogram scanner) ──────────────────
    src/il2cpp/il2cpp_resolver.cpp

    # ── Game actors: actor tracking, Time Hồi Chiêu, target selection ─────────
    src/game/game_actors.cpp
    # game.cpp = bridge: Game::heroCount/localInfo/heroes() for esp.cpp
    src/game/game.cpp

    # ── Feature config (globals + skin ID tables) ─────────────────────────────
    src/features/aov_config.cpp
    src/features/config_store.cpp

    # ── Features::tick() stub (satisfies esp.cpp linker ref) ──────────────────
    src/features/features_stub.cpp

    # ── Feature hooks ─────────────────────────────────────────────────────────
    src/features/misc_hooks.cpp          # Hack Map V2, camera zoom, name reveal
    src/features/aim_hooks.cpp           # Aimbot, Auto Bộc Phá, Auto Trừng Trị
    src/features/skin_mod.cpp            # Unlock All Skins (14 hooks)
    src/features/skin_apply.cpp          # Kill-notify + Button skin
    src/features/fps_hooks.cpp           # Unlock FPS 120

    # ── Subsystems ────────────────────────────────────────────────────────────
    src/hook/hook.cpp
    src/esp/esp.cpp
    src/menu/menu.cpp

    # ── Anti-cheat bypass ─────────────────────────────────────────────────────
    # Layer 3: Maps Hider, Layer 4: Thread Rename, Layer 5: Prop Spoofer
    src/anticheat/anticheat.cpp
)

# ── Shared Library Target ─────────────────────────────────────────────────────
add_library(aov_zygisk SHARED
    ${AOVMOD_SOURCES}
    ${IMGUI_SOURCES}
)

# ── Dobby Hook Library ────────────────────────────────────────────────────────
# Prebuilt static library — download from:
#   https://github.com/jmpews/Dobby/releases
# Or use the one from ALEX5402 repo:
#   module/src/main/cpp/libs/arm64-v8a/libdobby.a
# Place at: jni/vendor/dobby/lib/arm64-v8a/libdobby.a
add_library(dobby STATIC IMPORTED)
set_target_properties(dobby PROPERTIES
    IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/vendor/dobby/lib/${ANDROID_ABI}/libdobby.a
)

# ── Android System Libraries ──────────────────────────────────────────────────
find_library(log-lib     log)
find_library(android-lib android)
find_library(egl-lib     EGL)
find_library(gles2-lib   GLESv2)
find_library(gles3-lib   GLESv3)
find_library(dl-lib      dl)
find_library(z-lib       z)

# ── Link ──────────────────────────────────────────────────────────────────────
target_link_libraries(aov_zygisk
    dobby
    ${log-lib}
    ${android-lib}
    ${egl-lib}
    ${gles2-lib}
    ${gles3-lib}
    ${dl-lib}
    -static-libstdc++      # Statically link C++ runtime
)

# ── Build Info ────────────────────────────────────────────────────────────────
message(STATUS "=== AOV Zygisk Build Configuration ===")
message(STATUS "ABI:      ${ANDROID_ABI}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "NDK:      ${ANDROID_NDK}")
message(STATUS "Output:   aov_zygisk.so → zygisk/${ANDROID_ABI}.so")
