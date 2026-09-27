// =============================================================================
//  fps_hooks.cpp â€” Unlock FPS 120. Port of Mod's Fps.cpp.
//  Hooks 10 methods in GameSettings + GameAutoOptimizeFPS to force 120fps.
// =============================================================================

#include "include/aov_config.hpp"
#include "include/il2cpp_resolver.hpp"
#include "include/common.hpp"
#include "dobby.h"

static void (*m_set_targetFrameRate)(int) = nullptr;
static void (*m_set_vSyncCount)(int)      = nullptr;

// bool-returning capability getters â€” return true when unlock is on
static bool (*o_Sup60)(void*);  static bool hk_Sup60(void* s)  { return cfg.unlockFps120 ? true : o_Sup60(s);  }
static bool (*o_Sup90)(void*);  static bool hk_Sup90(void* s)  { return cfg.unlockFps120 ? true : o_Sup90(s);  }
static bool (*o_Sup120)(void*); static bool hk_Sup120(void* s) { return cfg.unlockFps120 ? true : o_Sup120(s); }
static bool (*o_SupBoth)(void*);static bool hk_SupBoth(void* s){ return cfg.unlockFps120 ? true : o_SupBoth(s);}
static bool (*o_IsIPad)(void*); static bool hk_IsIPad(void* s) { return cfg.unlockFps120 ? true : o_IsIPad(s); }

// CheckSupported90And120FPS_Android (argc 4): writes two out-bool params
static void (*o_Chk90_120)(void*,void*,void*,void*,void*);
static void hk_Chk90_120(void* s, void* a, void* out90, void* out120, void* d) {
    if (cfg.unlockFps120) { if (out90) *(unsigned char*)out90=1; if (out120) *(unsigned char*)out120=1; return; }
    o_Chk90_120(s, a, out90, out120, d);
}
// CheckDeviceSupport60FPS_Android (argc 1): out int* forces level 3
static int (*o_Chk60)(void*, void*);
static int hk_Chk60(void* outLevel, void* mi) {
    if (cfg.unlockFps120) { if (outLevel) *(int*)outLevel = 3; return 1; }
    return o_Chk60(outLevel, mi);
}
// LockFPSModeTo60FPS: unlock + pin 120
static void (*o_Lock60)(int, void*);
static void hk_Lock60(int arg, void* p2) {
    if (cfg.unlockFps120) {
        o_Lock60(0, p2);
        if (m_set_targetFrameRate) m_set_targetFrameRate(0x78);  // 120
        if (m_set_vSyncCount) m_set_vSyncCount(0);
    } else o_Lock60(arg, p2);
}
// GetDefaultFPSMode_Android: return 4 = 120fps mode
static int (*o_GetDefMode)(void*);
static int hk_GetDefMode(void* s) { return cfg.unlockFps120 ? 4 : o_GetDefMode(s); }
// CheckDropFPS: return 0 = never drop
static int (*o_CheckDrop)(void*);
static int hk_CheckDrop(void* s)  { return cfg.unlockFps120 ? 0 : o_CheckDrop(s); }

static void H(const char* dll, const char* ns, const char* cls, const char* m,
              int argc, void* det, void** orig) {
    if (*orig) return;
    void* p = (void*)Il2CppGetMethodOffset(dll, ns, cls, m, argc);
    if (p) DobbyHook(p, (void*)det, (void**)orig);
}

void InstallFpsHooks() {
    m_set_targetFrameRate = (void(*)(int))Il2CppGetMethodOffset("UnityEngine.CoreModule.dll", "UnityEngine", "Application",    "set_targetFrameRate", 1);
    m_set_vSyncCount      = (void(*)(int))Il2CppGetMethodOffset("UnityEngine.CoreModule.dll", "UnityEngine", "QualitySettings", "set_vSyncCount",      1);

    // Apply immediately if already enabled
    if (cfg.unlockFps120) {
        if (m_set_vSyncCount)      m_set_vSyncCount(0);
        if (m_set_targetFrameRate) m_set_targetFrameRate(0x78);
    }

    H(DLL_MAIN, NS_FRAME, "GameSettings",        "get_Supported60FPSMode",              0, (void*)hk_Sup60,     (void**)&o_Sup60);
    H(DLL_MAIN, NS_FRAME, "GameSettings",        "get_Supported90FPSMode",              0, (void*)hk_Sup90,     (void**)&o_Sup90);
    H(DLL_MAIN, NS_FRAME, "GameSettings",        "get_Supported120FPSMode",             0, (void*)hk_Sup120,    (void**)&o_Sup120);
    H(DLL_MAIN, NS_FRAME, "GameSettings",        "get_SupportedBoth60FPS_CameraHeight", 0, (void*)hk_SupBoth,   (void**)&o_SupBoth);
    H(DLL_MAIN, NS_FRAME, "GameSettings",        "IsIPadDevice",                        0, (void*)hk_IsIPad,    (void**)&o_IsIPad);
    H(DLL_MAIN, NS_FRAME, "GameSettings",        "CheckSupported90And120FPS_Android",   4, (void*)hk_Chk90_120, (void**)&o_Chk90_120);
    H(DLL_MAIN, NS_FRAME, "GameSettings",        "CheckDeviceSupport60FPS_Android",     1, (void*)hk_Chk60,     (void**)&o_Chk60);
    H(DLL_MAIN, NS_FRAME, "GameSettings",        "LockFPSModeTo60FPS",                  1, (void*)hk_Lock60,    (void**)&o_Lock60);
    H(DLL_MAIN, NS_FRAME, "GameSettings",        "GetDefaultFPSMode_Android",           0, (void*)hk_GetDefMode,(void**)&o_GetDefMode);
    H(DLL_MAIN, NS_FRAME, "GameAutoOptimizeFPS", "CheckDropFPS",                        0, (void*)hk_CheckDrop, (void**)&o_CheckDrop);
    LOGI("[fps] 120fps hooks installed (Sup60=%p Sup120=%p)", (void*)o_Sup60, (void*)o_Sup120);
}
