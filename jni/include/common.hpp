#pragma once
// =============================================================================
//  AOV Zygisk — Common Types, Macros & Math Helpers
//  Target: Arena of Valor (Liên Quân Mobile) com.garena.game.kgvn
//  Engine: Unity (libmain.so + libunity.so)
// =============================================================================
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <cstring>
#include <string>
#include <android/log.h>

// ── Logging ──────────────────────────────────────────────────────────────────
#define LOG_TAG "aov_zygisk"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ── Target App ───────────────────────────────────────────────────────────────
#define TARGET_PKG          "com.garena.game.kgvn"
#define TARGET_LIB_MAIN     "libmain.so"
#define TARGET_LIB_UNITY    "libunity.so"

// ── Math Types ───────────────────────────────────────────────────────────────
struct Vector2 {
    float x = 0.f, y = 0.f;
    Vector2() = default;
    Vector2(float x, float y) : x(x), y(y) {}
    float length() const { return sqrtf(x*x + y*y); }
    Vector2 operator-(const Vector2& o) const { return {x-o.x, y-o.y}; }
    Vector2 operator+(const Vector2& o) const { return {x+o.x, y+o.y}; }
    Vector2 operator*(float s) const { return {x*s, y*s}; }
};

struct Vector3 {
    float x = 0.f, y = 0.f, z = 0.f;
    Vector3() = default;
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
    float length() const { return sqrtf(x*x + y*y + z*z); }
    float lengthSq() const { return x*x + y*y + z*z; }
    float distance(const Vector3& o) const { return (*this - o).length(); }
    Vector3 normalized() const {
        float l = length();
        return l > 0.f ? Vector3{x/l, y/l, z/l} : Vector3{};
    }
    Vector3 operator-(const Vector3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    Vector3 operator+(const Vector3& o) const { return {x+o.x, y+o.y, z+o.z}; }
    Vector3 operator*(float s) const { return {x*s, y*s, z*s}; }
    float dot(const Vector3& o) const { return x*o.x + y*o.y + z*o.z; }
    bool isValid() const { return std::isfinite(x) && std::isfinite(y) && std::isfinite(z); }
    bool isZero() const { return x == 0.f && y == 0.f && z == 0.f; }
};

struct Matrix4x4 {
    float m[4][4] = {};
    // Row-major projection for Unity WorldToScreen
    Vector3 multiplyPoint(const Vector3& v) const {
        float w = m[3][0]*v.x + m[3][1]*v.y + m[3][2]*v.z + m[3][3];
        if (fabsf(w) < 1e-6f) return {};
        float x = (m[0][0]*v.x + m[0][1]*v.y + m[0][2]*v.z + m[0][3]) / w;
        float y = (m[1][0]*v.x + m[1][1]*v.y + m[1][2]*v.z + m[1][3]) / w;
        float z = (m[2][0]*v.x + m[2][1]*v.y + m[2][2]*v.z + m[2][3]) / w;
        return {x, y, z};
    }
};

// ── Memory Helpers ───────────────────────────────────────────────────────────
template<typename T>
inline T readMem(uintptr_t addr) {
    if (addr < 0x1000) return T{};
    return *reinterpret_cast<T*>(addr);
}

template<typename T>
inline void writeMem(uintptr_t addr, T val) {
    if (addr < 0x1000) return;
    *reinterpret_cast<T*>(addr) = val;
}

inline bool isValidPtr(uintptr_t p) {
    // Every valid pointer in this arm64 Android process lives above 4 GiB.
    // Rejecting small non-null values is important during IL2CPP pool
    // teardown, where released handles can temporarily contain IDs/RVAs.
    return (p >= 0x100000000ull && p < 0x7FFFFFFFFFFFull);
}

// ── HOOK_RVA macro ───────────────────────────────────────────────────────────
#define HOOK_RVA(base, rva, hook, orig) \
    Hook::hookAt(reinterpret_cast<void*>((base) + (rva)), \
                 reinterpret_cast<void*>(hook), \
                 reinterpret_cast<void**>(&orig))
