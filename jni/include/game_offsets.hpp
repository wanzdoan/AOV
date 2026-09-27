#pragma once
// =============================================================================
//  game_offsets.hpp — Unity Managed Object Layout Constants
//
//  NOTE: As of v2.6.0, ALL method/field offsets are resolved at runtime via
//  the IL2Cpp by-name resolver (il2cpp_resolver.cpp). This file only contains
//  STABLE Unity runtime struct layout constants that do NOT change between
//  game versions (they are Unity engine internals, not game code).
//
//  The old 1.63.1.10 hardcoded RVA offsets have been REMOVED.
//  Target game version: com.garena.game.kgvn 1.63.1.14
//  Resolution approach: IL2Cpp by-name API (see include/il2cpp_resolver.hpp)
// =============================================================================

#include <cstdint>

namespace GameOffsets {

constexpr const char* LIB_IL2CPP = "libil2cpp.so";

// ── Unity Managed Object / Collection Layout ──────────────────────────────────
// These are Unity runtime constants — stable across game versions.
// 8-byte object header: klass pointer (8 bytes) + monitor pointer (8 bytes).
constexpr uintptr_t List_items        = 0x08;   // List<T>._items (T[])
constexpr uintptr_t List_size         = 0x10;   // List<T>._size (int)
constexpr uintptr_t Array_length      = 0x10;   // T[].Length (int, via il2cpp header)
constexpr uintptr_t Array_data        = 0x18;   // T[].m_Items[0] (first element)
constexpr uintptr_t PoolHandle_object = 0x08;   // PoolObjHandle<T>.Object (T*)
constexpr uintptr_t PoolHandle_stride = 0x10;   // stride of handle in array (16 bytes)
constexpr uintptr_t String_length     = 0x08;   // System.String.m_stringLength (int)
constexpr uintptr_t String_chars      = 0x0C;   // System.String.m_firstChar (char*)

// ── Fixed-point coordinate precision (VInt → float) ─────────────────────────
constexpr float VIntPrecision = 1000.0f;  // VInt coords are x1000 of float

} // namespace GameOffsets
