#pragma once
// =============================================================================
//  AOV Zygisk — Legacy Offsets Shim
//
//  Offsets game THẬT nằm ở include/game_offsets.hpp (từ dump.cs 1.63.1.10).
//  File này chỉ giữ lại những gì code cũ còn dùng:
//   - InjectEvent_Pattern: fallback pattern cho Unity cũ (Unity 2022.3 của AOV
//     không match — touch chính đi qua nativeInjectEvent, xem esp.cpp).
// =============================================================================
#include <EGL/egl.h>

namespace Offsets {

// ── EGL Constants ─────────────────────────────────────────────────────────────
constexpr EGLint EGL_WIDTH_ATTR  = EGL_WIDTH;
constexpr EGLint EGL_HEIGHT_ATTR = EGL_HEIGHT;

// ── libunity.so — inject_event byte pattern (LEGACY fallback) ─────────────────
// Pattern từ ALEX5402 (Unity 2019-ish). AOV dùng Unity 2022.3.5f1 →
// findPattern() sẽ không thấy (đã xác nhận trên máy) và Esp::init log warn.
// Touch thật: UnityPlayer.nativeInjectEvent qua chặn RegisterNatives.
constexpr const char* InjectEvent_Pattern =
    "FF 83 01 D1 F7 5B 03 A9 F5 53 04 A9 F3 7B 05 A9 F5 03 02 AA";

} // namespace Offsets
