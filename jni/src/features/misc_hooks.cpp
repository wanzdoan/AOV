// =============================================================================
//  misc_hooks.cpp â€” Map hack V2 (LVActorLinker.SetVisible), name reveal,
//  camera zoom (CameraSystem). Port of Mod's MiscHooks.cpp.
//  REPLACES the old features.cpp (fog + zoom).
// =============================================================================

#include "include/game_actors.hpp"
#include "include/aov_config.hpp"
#include "include/il2cpp_resolver.hpp"
#include "include/common.hpp"
#include "dobby.h"

// ---- Hack Map V2: LVActorLinker.SetVisible ----------------------------------
static void (*o_SetVisible)(void*, unsigned long, unsigned, unsigned) = nullptr;
static void hk_SetVisible(void* actor, unsigned long visType, unsigned vis, unsigned p4) {
    if (actor && cfg.hackMapV2) {
        int t = (int)visType;
        if (((unsigned)(t - 1) < 2) || t == 0xff || t == 0x6e) vis = 1; // force visible
    }
    o_SetVisible(actor, visType, vis & 1, p4 & 1);
}

// ---- CPlayerProfile.get_IsHostProfile â†’ reveal enemy name/info --------------
static bool (*o_IsHostProfile)(void*) = nullptr;
static bool hk_IsHostProfile(void* self) {
    if (self && IsShowNameInfo) return true;
    return o_IsHostProfile(self);
}

// ---- Camera zoom (Pháº¡m Vi): CameraSystem -----------------------------------
static void  (*m_OnCameraHeightChanged)(void*) = nullptr;
static void  (*o_CameraUpdate)(void*)          = nullptr;
static float (*o_GetCamHeightRate)(void*, void*) = nullptr;

static void hk_CameraUpdate(void* self) {
    if (self && cfg.rangeEnable && m_OnCameraHeightChanged) m_OnCameraHeightChanged(self);
    o_CameraUpdate(self);
}
static float hk_GetCamHeightRate(void* self, void* a) {
    if (self && cfg.rangeEnable) {
        float v = o_GetCamHeightRate(self, a);
        if (cfg.rangeValue != 0.0f) v += cfg.rangeValue;
        return v;
    }
    return o_GetCamHeightRate(self, a);
}

// ============================================================
//  InstallMiscHooks
// ============================================================
void InstallMiscHooks() {
    // Hack Map V2 â€” guard on orig pointer (retry loop safe)
    if (!o_SetVisible)
        if (void* p = (void*)Il2CppGetMethodOffset(DLL_PLUG, NS_NLOGIC, "LVActorLinker", "SetVisible", 3))
            DobbyHook(p, (void*)hk_SetVisible, (void**)&o_SetVisible);

    // Enemy name reveal (IsHostProfile)
    if (!o_IsHostProfile)
        if (void* p = (void*)Il2CppGetMethodOffset(DLL_MAIN, NS_SYSTEM, "CPlayerProfile", "get_IsHostProfile", 0))
            DobbyHook(p, (void*)hk_IsHostProfile, (void**)&o_IsHostProfile);

    // Camera Pháº¡m Vi zoom
    if (!m_OnCameraHeightChanged)
        m_OnCameraHeightChanged = (void(*)(void*))Il2CppGetMethodOffset(DLL_MAIN, "", "CameraSystem", "OnCameraHeightChanged", 0);
    if (!o_CameraUpdate)
        if (auto p = (void*)Il2CppGetMethodOffset(DLL_MAIN, "", "CameraSystem", "Update", 0))
            DobbyHook(p, (void*)hk_CameraUpdate, (void**)&o_CameraUpdate);
    if (!o_GetCamHeightRate)
        if (auto p = (void*)Il2CppGetMethodOffset(DLL_MAIN, "", "CameraSystem", "GetCameraHeightRateValue", 1))
            DobbyHook(p, (void*)hk_GetCamHeightRate, (void**)&o_GetCamHeightRate);

    LOGI("[misc] map=%p profile=%p camUpdate=%p camRate=%p",
         (void*)o_SetVisible, (void*)o_IsHostProfile, (void*)o_CameraUpdate, (void*)o_GetCamHeightRate);
}
