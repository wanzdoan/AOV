// =============================================================================
//  skin_mod.cpp â€” Unlock All Skins feature. Port of Mod's SkinMod.cpp.
//  14 hooks that redirect skin selection, resource loading, and sound.
//  Only redirects for the hero+skin YOU select in hero-select â€” never forces
//  all skins to load (which causes match-load hang on undownloaded assets).
// =============================================================================

#include "include/game_actors.hpp"
#include "include/aov_config.hpp"
#include "include/il2cpp_resolver.hpp"
#include "include/common.hpp"
#include "dobby.h"
#include <cstdint>

static bool g_skinHooked = false;

// ---- Selection state --------------------------------------------------------
static uint32_t g_selHero  = 0;   // selected hero id
static uint32_t g_selLocal = 0;   // selected local skin index
static uint32_t g_selCfg   = 0;   // selected full cfg id (hero*100 + index)
static uint32_t g_selRaw   = 0;   // raw skin param
static int32_t  g_realCfg  = 0;   // actor's real cfg id from server

static void computeSel(int heroId, uint32_t skinId) {
    uint32_t base = (uint32_t)heroId * 100;
    bool lo = (heroId == 0 || skinId < base);
    g_selHero  = (uint32_t)heroId;
    g_selLocal = lo ? skinId : (skinId - base);
    g_selCfg   = lo ? (base + skinId) : skinId;
    g_selRaw   = skinId;
}
static uint32_t realLocal() {
    if (g_selHero == 0) return 0;
    uint32_t base = g_selHero * 100;
    return ((uint32_t)g_realCfg >= base) ? ((uint32_t)g_realCfg - base) : 0;
}
static bool isSel(int heroId, int skinId) {
    if (g_selHero != (uint32_t)heroId || g_selHero == 0 || g_realCfg == 0) return false;
    if ((int)realLocal() == skinId) return true;
    if ((int)g_selLocal  == skinId) return true;
    return (int)g_selCfg == skinId;
}

// ---- Cached (not hooked) methods for UI refresh ----------------------------
static void (*m_RefreshHeroPanel)(void*, int, int, int)  = nullptr;
static void (*m_OnHeroSkinWearSuc)(void*, int, int)      = nullptr;
static void (*m_RefreshSelectHeroInfo)(void*)            = nullptr;

// ---- Hook originals ---------------------------------------------------------
static uint64_t (*o_GetHeroWearSkinId)(void*, int)                       = nullptr;
static void     (*o_OnClickSelectHeroSkin)(void*, int, int)               = nullptr;
static void     (*o_OnSkinSelect)(void*, void*)                           = nullptr;
static uint64_t (*o_IsCanUseSkin)(void*, int, int)                        = nullptr;
static uint64_t (*o_IsHaveHeroSkin)(void*, int, int, int)                 = nullptr;
static void     (*o_ConvertServerHeroInfo)(void*, void*, void*, void*)    = nullptr;
static void*    (*o_GetHeroSkin)(int, int)                                = nullptr;
static uint64_t (*o_GetSkinCfgId)(uint64_t, uint64_t)                    = nullptr;
static void     (*o_ActorGetSkin)(void*, uint64_t)                        = nullptr;
static uint64_t (*o_GetOriSkinId)(uint64_t, int)                          = nullptr;
static uint64_t (*o_IsSkinAvailable)(void*, int)                          = nullptr;
static void     (*o_GetSkinResourcePath)(int, uint32_t, void*)            = nullptr;
static bool     (*o_get_useSkinSwitch)(void*)                             = nullptr;
static void     (*o_SoundTick_OnStart)(void*)                             = nullptr;

// ---- Detours ----------------------------------------------------------------
static uint64_t hk_GetHeroWearSkinId(void* self, int heroId) {
    if (cfg.unlockAllSkins && g_selHero == (uint32_t)heroId) return g_selRaw;
    return o_GetHeroWearSkinId(self, heroId);
}
static void hk_OnClickSelectHeroSkin(void* self, int heroId, int skinId) {
    computeSel(heroId, (uint32_t)skinId); g_realCfg = 0;
    o_OnClickSelectHeroSkin(self, heroId, skinId);
    if (m_RefreshHeroPanel) m_RefreshHeroPanel(self, 0, 0, 0);
}
static void hk_OnSkinSelect(void* self, void* params) {
    if (params) {
        uintptr_t o16 = IL2Cpp::Il2CppGetFieldOffset(DLL_MAIN, "Assets.Scripts.UI", "stUIEventParams", "commonUInt16Param1");
        uintptr_t o64 = IL2Cpp::Il2CppGetFieldOffset(DLL_MAIN, "Assets.Scripts.UI", "stUIEventParams", "commonUInt64Param1");
        short hero = o16 ? *(short*)((char*)params + o16) : 0;
        int   skin = o64 ? *(int*)((char*)params + o64)   : 0;
        if (hero != 0 && skin != 0) {
            computeSel(hero, (uint32_t)skin); g_realCfg = 0;
            if (m_OnHeroSkinWearSuc)     m_OnHeroSkinWearSuc(self, hero, skin);
            if (m_RefreshSelectHeroInfo) m_RefreshSelectHeroInfo(self);
        }
    }
}
static uint64_t hk_IsCanUseSkin(void* self, int a, int b)     { if (cfg.unlockAllSkins) return 1; return o_IsCanUseSkin(self, a, b); }
static uint64_t hk_IsHaveHeroSkin(void* self, int a, int b, int c) { if (cfg.unlockAllSkins) return 1; return o_IsHaveHeroSkin(self, a, b, c); }
static void hk_ConvertServerHeroInfo(void* self, void* p2, void* info, void* p4) {
    using namespace IL2Cpp;
    if (info && g_selHero != 0) {
        void* baseInfo = GetFieldObj(info, "stBaseInfo");
        void* common   = baseInfo ? GetFieldObj(baseInfo, "stCommonInfo") : nullptr;
        if (common) {
            uint32_t heroId = GetField<uint32_t>(common, "dwHeroID");
            if (heroId == g_selHero) {
                uint16_t realSkin = GetField<uint16_t>(common, "wSkinID");
                int cfgid = (int)g_selHero * 100 + realSkin;
                if (g_realCfg == 0 || cfgid != (int)g_selCfg) g_realCfg = cfgid;
                SetField<uint16_t>(common, "wSkinID", (uint16_t)g_selLocal);
            }
        }
    }
    o_ConvertServerHeroInfo(self, p2, info, p4);
}
static void* hk_GetHeroSkin(int heroId, int skinId) {
    void* obj = o_GetHeroSkin(heroId, skinId);
    if (obj && cfg.unlockAllSkins && isSel(heroId, skinId))
        IL2Cpp::SetField<uint32_t>(obj, "dwID", g_selCfg);
    return obj;
}
static uint64_t hk_GetSkinCfgId(uint64_t heroId, uint64_t skinId) {
    if (cfg.unlockAllSkins) {
        int h = (int)(heroId & 0xffffffff), s = (int)(skinId & 0xffffffff);
        if (isSel(h, s)) return g_selCfg;
    }
    return o_GetSkinCfgId(heroId & 0xffffffff, skinId & 0xffffffff);
}
static void hk_ActorGetSkin(void* self, uint64_t skinId) {
    uint64_t use = skinId;
    if (cfg.unlockAllSkins && g_selHero != 0 && g_realCfg != 0) {
        uint32_t sid = (uint32_t)skinId, rl = realLocal(), u = g_selLocal;
        if (g_selLocal != sid && rl != sid) u = sid;
        use = u;
    }
    o_ActorGetSkin(self, use);
}
static uint64_t hk_GetOriSkinId(uint64_t a, int heroId) {
    if (!cfg.unlockAllSkins || g_selHero == 0 || g_selHero != (uint32_t)heroId) return o_GetOriSkinId(a, heroId);
    return g_selLocal;
}
static uint64_t hk_IsSkinAvailable(void* self, int cfgId) {
    if (!cfg.unlockAllSkins || g_selHero == 0 || g_selCfg != (uint32_t)cfgId) return o_IsSkinAvailable(self, cfgId);
    return 1;
}
static void hk_GetSkinResourcePath(int heroId, uint32_t skinId, void* p3) {
    uint32_t use = g_selLocal;
    if (g_selHero != (uint32_t)heroId || g_selHero == 0 || !cfg.unlockAllSkins) use = skinId;
    o_GetSkinResourcePath(heroId, use, p3);
}
static bool hk_get_useSkinSwitch(void* self) { if (cfg.unlockAllSkins) return true; return o_get_useSkinSwitch(self); }
static void hk_SoundTick_OnStart(void* self) {
    using namespace IL2Cpp;
    if (self && g_selHero != 0) {
        void* sd = GetFieldObj(self, "StaticData");
        if (sd && !GetField<bool>(sd, "useSkinSwitch")) SetField<bool>(sd, "useSkinSwitch", true);
    }
    o_SoundTick_OnStart(self);
}

// ---- Install macro (guard on orig pointer) ----------------------------------
#define HOOKM(dll,ns,cls,mtd,argc,det,orig) \
    do { if (!(orig)) { auto _p=(void*)Il2CppGetMethodOffset(dll,ns,cls,mtd,argc); \
             if (_p) DobbyHook(_p,(void*)(det),(void**)&(orig)); } } while(0)

void SkinMod_InstallHooks() {
    if (g_skinHooked) return;

    m_RefreshHeroPanel      = (void(*)(void*,int,int,int))Il2CppGetMethodOffset(DLL_MAIN, NS_SYSTEM, "HeroSelectNormalWindow", "RefreshHeroPanel", 3);
    m_OnHeroSkinWearSuc     = (void(*)(void*,int,int))    Il2CppGetMethodOffset(DLL_MAIN, NS_SYSTEM, "HeroSelectBanPickWindow", "OnHeroSkinWearSuc", 2);
    m_RefreshSelectHeroInfo = (void(*)(void*))            Il2CppGetMethodOffset(DLL_MAIN, NS_SYSTEM, "HeroSelectBanPickWindow", "RefreshSelectHeroInfo", 0);

    HOOKM(DLL_MAIN, NS_SYSTEM, "CRoleInfo",               "GetHeroWearSkinId",       1, hk_GetHeroWearSkinId,       o_GetHeroWearSkinId);
    HOOKM(DLL_MAIN, NS_SYSTEM, "HeroSelectNormalWindow",  "OnClickSelectHeroSkin",   2, hk_OnClickSelectHeroSkin,   o_OnClickSelectHeroSkin);
    HOOKM(DLL_MAIN, NS_SYSTEM, "HeroSelectBanPickWindow", "HeroSelect_OnSkinSelect", 1, hk_OnSkinSelect,            o_OnSkinSelect);
    HOOKM(DLL_MAIN, NS_SYSTEM, "CRoleInfo",               "IsCanUseSkin",            2, hk_IsCanUseSkin,            o_IsCanUseSkin);
    HOOKM(DLL_MAIN, NS_SYSTEM, "CRoleInfo",               "IsHaveHeroSkin",          3, hk_IsHaveHeroSkin,          o_IsHaveHeroSkin);
    HOOKM(DLL_PLUG, "LDataProvider", "ActorServerDataProvider", "ConvertServerHeroInfo", 3, hk_ConvertServerHeroInfo, o_ConvertServerHeroInfo);
    HOOKM(DLL_MAIN, NS_SYSTEM, "CSkinInfo",               "GetHeroSkin",             2, hk_GetHeroSkin,             o_GetHeroSkin);
    HOOKM(DLL_MAIN, NS_SYSTEM, "CSkinInfo",               "GetSkinCfgId",            2, hk_GetSkinCfgId,            o_GetSkinCfgId);
    HOOKM(DLL_MAIN, NS_LOGIC,  "CActorInfo",              "GetSkin",                 1, hk_ActorGetSkin,            o_ActorGetSkin);
    HOOKM(DLL_MAIN, NS_LOGIC,  "SkinResourceHelper",      "GetOriSkinId",            2, hk_GetOriSkinId,            o_GetOriSkinId);
    HOOKM(DLL_MAIN, NS_FRAME,  "GameDataMgr",             "IsSkinAvailable",         1, hk_IsSkinAvailable,         o_IsSkinAvailable);
    HOOKM(DLL_MAIN, NS_LOGIC,  "SkinResourceHelper",      "GetSkinResourcePath",     3, hk_GetSkinResourcePath,     o_GetSkinResourcePath);
    HOOKM(DLL_PLUG, "NucleusDrive.Logic.LAGE", "PlayHeroSoundTick", "get_useSkinSwitch", 0, hk_get_useSkinSwitch, o_get_useSkinSwitch);
    HOOKM(DLL_MAIN, NS_VAGE,   "PlayHeroSoundTick",       "OnStart",                 0, hk_SoundTick_OnStart,       o_SoundTick_OnStart);

    if (o_IsHaveHeroSkin && o_IsCanUseSkin) {
        g_skinHooked = true;
        LOGI("[skin] 14 hooks installed");
    } else {
        LOGW("[skin] some hooks failed â€” will retry next pass");
    }
}

// Called by menu when unlockAllSkins toggle changes (no explicit refresh needed)
void RefreshSkins() {}
