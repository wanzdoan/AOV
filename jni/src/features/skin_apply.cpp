// =============================================================================
//  skin_apply.cpp â€” Apply kill-notify billboard + button skins.
//  Port of Mod's SkinApply.cpp. 2 hooks:
//   - LobbyLogic.UpdateLogic â†’ cache g_myAccountUid
//   - GamePlayerCenter.GetPlayer â†’ patch broadcastID + PersonalButtonID
// =============================================================================

#include "include/game_actors.hpp"
#include "include/aov_config.hpp"
#include "include/il2cpp_resolver.hpp"
#include "include/common.hpp"
#include "dobby.h"

static void* (*o_GetPlayer)(void*, unsigned long)  = nullptr;
static void  (*o_LobbyUpdate)(void*, unsigned)     = nullptr;

// GamePlayerCenter.GetPlayer â†’ patch local player's skin IDs
static void* hk_GetPlayer(void* self, unsigned long uid) {
    void* pb = o_GetPlayer(self, uid);
    if (!pb || (cfg.killNotifySkinId <= 0 && cfg.buttonSkinId <= 0)) return pb;
    static uintptr_t fUid = 0, fBroadcast = 0, fButton = 0;
    static bool resolved = false;
    if (!resolved) {
        fUid       = IL2Cpp::Il2CppGetFieldOffset(DLL_PLUG, "LDataProvider", "PlayerBase", "PlayerUId");
        fBroadcast = IL2Cpp::Il2CppGetFieldOffset(DLL_PLUG, "LDataProvider", "PlayerBase", "broadcastID");
        fButton    = IL2Cpp::Il2CppGetFieldOffset(DLL_PLUG, "LDataProvider", "PlayerBase", "PersonalButtonID");
        if (fUid && fBroadcast && fButton) resolved = true;
    }
    if (fUid && *(uint64_t*)((char*)pb + fUid) == g_myAccountUid) {
        if (cfg.killNotifySkinId > 0 && fBroadcast) *(int*)((char*)pb + fBroadcast) = cfg.killNotifySkinId;
        if (cfg.buttonSkinId    > 0 && fButton)     *(int*)((char*)pb + fButton)    = cfg.buttonSkinId;
    }
    return pb;
}

// LobbyLogic.UpdateLogic â†’ cache local account uid
static void hk_LobbyUpdate(void* self, unsigned p2) {
    if (self) {
        uintptr_t f = IL2Cpp::Il2CppGetFieldOffset(DLL_MAIN, NS_LOGIC, "LobbyLogic", "ulAccountUid");
        if (f) { uint64_t u = *(uint64_t*)((char*)self + f); if (u) g_myAccountUid = u; }
    }
    o_LobbyUpdate(self, p2);
}

void InstallSkinApplyHooks() {
    static bool s_done = false; if (s_done) return;
    void* pL = (void*)IL2Cpp::Il2CppGetMethodOffset(DLL_MAIN, NS_LOGIC,   "LobbyLogic",       "UpdateLogic", 1);
    void* pG = (void*)IL2Cpp::Il2CppGetMethodOffset(DLL_PLUG, NS_KERNAL,  "GamePlayerCenter", "GetPlayer",   1);
    if (pL) DobbyHook(pL, (void*)hk_LobbyUpdate, (void**)&o_LobbyUpdate);
    if (pG) DobbyHook(pG, (void*)hk_GetPlayer,   (void**)&o_GetPlayer);
    if (pL && pG) s_done = true;
    LOGI("[skin_apply] LobbyUpdate=%p GetPlayer=%p", pL, pG);
}
