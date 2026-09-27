// =============================================================================
//  aim_hooks.cpp â€” Aimbot + Auto Bá»™c PhÃ¡ + Auto Trá»«ng Trá»‹ hook installation.
//  Port of Mod's Aim.cpp. Hooks:
//   - SkillControlIndicator.GetUseSkillPosition/Direction â†’ aimbot
//   - SkillSlot.LateUpdate â†’ auto-cast Bá»™c PhÃ¡ / Trá»«ng Trá»‹ (slot 5)
//   - CSkillButtonManager.UpdateLogic â†’ track isCharging/skillSlot
//   - PunishPromptDuration.get_isHpUnderPunishValue â†’ cache Killm
// =============================================================================

#include "include/game_actors.hpp"
#include "include/aov_config.hpp"
#include "include/il2cpp_resolver.hpp"
#include "include/common.hpp"
#include "dobby.h"
#include <cstdint>
#include <cmath>

// ---- Cached field offsets ---------------------------------------------------
static uintptr_t off_SCI_skillSlot   = 0;
static uintptr_t off_SlotType        = 0;
static uintptr_t off_CSBM_isCharging = 0;
static uintptr_t off_CSBM_slotType   = 0;

// ---- Cached method pointers (not hooked, called) ----------------------------
static void (*m_RequestUseSkill)(void*)     = nullptr;
static void (*m_ReadyUseSkill)(void*, int)  = nullptr;

// ---- Hook trampolines -------------------------------------------------------
static Vec3  (*orig_GetUseSkillPosition)(void*, bool)  = nullptr;
static Vec3  (*orig_GetUseSkillDirection)(void*, bool) = nullptr;
static void  (*orig_SCI_LateUpdate)(void*, int)        = nullptr;
static void  (*orig_SkillSlot_LateUpdate)(void*, int)  = nullptr;
static void  (*orig_UpdateLogic)(void*, int)           = nullptr;
static bool  (*orig_Punish)(void*)                     = nullptr;

// ============================================================
//  Aim math: calculateSkillPosition
//  out = enemy; if(|lead|>0.05) out = enemy + lead*range;
//  if(maxDist>0 && maxDist < dist(self,out)) clamp to maxDist.
// ============================================================
static Vec3 calcSkillPosition(Vec3 self, Vec3 ep, Vec3 lead, float range, float maxDist) {
    Vec3 out = ep;
    float leadMag = sqrtf(lead.x*lead.x + lead.y*lead.y + lead.z*lead.z);
    if (leadMag > 0.05f) {
        out.x = ep.x + lead.x * range;
        out.y = ep.y + lead.y * range;
        out.z = ep.z + lead.z * range;
    }
    float dx = out.x-self.x, dy = out.y-self.y, dz = out.z-self.z;
    float d = sqrtf(dx*dx + dy*dy + dz*dz);
    if (maxDist > 0.0f && maxDist < d && d > 0.0f) {
        float s = maxDist / d;
        out.x = self.x + dx * s;
        out.y = self.y + dy * s;
        out.z = self.z + dz * s;
    }
    return out;
}

// Select the target buffer + per-slot toggle by skill SlotType (1=C1,2=C2,3=Ulti)
static bool pickSlot(void* indicator, float*& t) {
    if (!indicator || !AimSkill || !off_SCI_skillSlot || !off_SlotType) return false;
    void* slot = *(void**)((char*)indicator + off_SCI_skillSlot);
    if (!slot) return false;
    int slotType = *(int*)((char*)slot + off_SlotType);
    switch (slotType) {
        case 1: if (!AimSkill1) return false; t = EnemyTarget1; return true;
        case 2: if (!AimSkill2) return false; t = EnemyTarget2; return true;
        case 3: if (!AimSkill3) return false; t = EnemyTarget3; return true;
    }
    return false;
}

// ---- GetUseSkillPosition detour â€” replace return value with aimed position --
static Vec3 hk_GetUseSkillPosition(void* indicator, bool arg) {
    float* t;
    if (pickSlot(indicator, t) && t[10] > 0.0f) {
        Vec3 self{t[0],t[1],t[2]}, ep{t[3],t[4],t[5]}, lead{t[6],t[7],t[8]};
        if (!vec3IsZero(self) && !vec3IsZero(ep))
            return calcSkillPosition(self, ep, lead, t[10], t[11]);
    }
    return orig_GetUseSkillPosition(indicator, arg);
}

// ---- GetUseSkillDirection detour â€” replace with aimed direction vector ------
static Vec3 hk_GetUseSkillDirection(void* indicator, bool arg) {
    float* t;
    if (pickSlot(indicator, t) && t[10] > 0.0f) {
        Vec3 self{t[0],t[1],t[2]}, ep{t[3],t[4],t[5]}, lead{t[6],t[7],t[8]};
        if (!vec3IsZero(self) && !vec3IsZero(ep)) {
            if (t[11] == 0.0f || vec3Dist(self, ep) <= t[11]) {
                Vec3 aim = calcSkillPosition(self, ep, lead, t[10], t[11]);
                float dx = aim.x-self.x, dy = aim.y-self.y, dz = aim.z-self.z;
                float len = sqrtf(dx*dx+dy*dy+dz*dz);
                if (len > 0.0f) return {dx/len, dy/len, dz/len};
            }
        }
    }
    return orig_GetUseSkillDirection(indicator, arg);
}

// ---- SCI.LateUpdate passthrough (keeps trampoline valid) --------------------
static void hk_SCI_LateUpdate(void* s, int a) { orig_SCI_LateUpdate(s, a); }

// ---- CSkillButtonManager.UpdateLogic â€” publish isCharging/skillSlot ---------
static void hk_UpdateLogic(void* mgr, int a) {
    if (mgr && AimSkill) {
        if (!off_CSBM_isCharging) {
            off_CSBM_isCharging = Il2CppGetFieldOffset(DLL_MAIN, NS_SYSTEM, "CSkillButtonManager", "m_isCharging");
            off_CSBM_slotType   = Il2CppGetFieldOffset(DLL_MAIN, NS_SYSTEM, "CSkillButtonManager", "m_currentSkillSlotType");
        }
        if (off_CSBM_isCharging) isCharging = *(uint8_t*)((char*)mgr + off_CSBM_isCharging) != 0;
        if (off_CSBM_slotType)   skillSlot  = *(int*)((char*)mgr + off_CSBM_slotType);
    }
    orig_UpdateLogic(mgr, a);
}

// ---- PunishPromptDuration.get_isHpUnderPunishValue â€” cache Killm for Trá»«ng Trá»‹
static bool hk_Punish(void* s) {
    bool v = orig_Punish(s);
    if (s && Kstt) Killm = v;
    return v;
}

// ---- SkillSlot.LateUpdate â€” auto-cast slot-5 for Bá»™c PhÃ¡/Trá»«ng Trá»‹ ---------
static void hk_SkillSlot_LateUpdate(void* slot, int a) {
    if (slot && off_SlotType) {
        int st = *(int*)((char*)slot + off_SlotType);
        // Slot 5 = phá»¥ trá»£ spell (Bá»™c PhÃ¡ / Trá»«ng Trá»‹)
        if (st == 5 && m_RequestUseSkill && m_ReadyUseSkill) {
            if (Willbp) { m_ReadyUseSkill(slot, a); m_RequestUseSkill(slot); Willbp = false; }
            if (Willtt) { m_ReadyUseSkill(slot, a); m_RequestUseSkill(slot); Willtt = false; }
        }
        // Slot 9 = auto self-cast when HP < Pthp% (if configured)
        if (st == 9 && liveActor(g_myActor) && Ksbp && m_RequestUseSkill && Pthp > 0.0f) {
            void* v = Actor_ValueComponent(g_myActor);
            int hp = Value_get_actorHp(v), tot = Value_get_actorHpTotal(v);
            if (tot > 0 && ((float)hp / tot * 100.0f) < Pthp) m_RequestUseSkill(slot);
        }
    }
    orig_SkillSlot_LateUpdate(slot, a);
}

// ============================================================
//  InitAimHooks â€” install all aim + auto-cast hooks
// ============================================================
void InitAimHooks() {
    // Resolve field offsets
    off_SCI_skillSlot = Il2CppGetFieldOffset(DLL_MAIN, NS_LOGIC, "SkillControlIndicator", "skillSlot");
    off_SlotType      = Il2CppGetFieldOffset(DLL_MAIN, NS_LOGIC, "SkillSlot", "SlotType");
    if (!m_RequestUseSkill)
        m_RequestUseSkill = (void(*)(void*))   Il2CppGetMethodOffset(DLL_MAIN, NS_LOGIC, "SkillSlot", "RequestUseSkill", 0);
    if (!m_ReadyUseSkill)
        m_ReadyUseSkill   = (void(*)(void*,int))Il2CppGetMethodOffset(DLL_MAIN, NS_LOGIC, "SkillSlot", "ReadyUseSkill", 1);
    Pthp = (float)GetHackConfig()->pthp;

    // Install hooks (guard on orig pointer â€” never re-hook)
    if (!orig_GetUseSkillPosition)
        if (auto p = (void*)Il2CppGetMethodOffset(DLL_MAIN, NS_LOGIC, "SkillControlIndicator", "GetUseSkillPosition", 1))
            DobbyHook(p, (void*)hk_GetUseSkillPosition, (void**)&orig_GetUseSkillPosition);
    if (!orig_GetUseSkillDirection)
        if (auto p = (void*)Il2CppGetMethodOffset(DLL_MAIN, NS_LOGIC, "SkillControlIndicator", "GetUseSkillDirection", 1))
            DobbyHook(p, (void*)hk_GetUseSkillDirection, (void**)&orig_GetUseSkillDirection);
    if (!orig_UpdateLogic)
        if (auto p = (void*)Il2CppGetMethodOffset(DLL_MAIN, NS_SYSTEM, "CSkillButtonManager", "UpdateLogic", 1))
            DobbyHook(p, (void*)hk_UpdateLogic, (void**)&orig_UpdateLogic);
    if (!orig_Punish)
        if (auto p = (void*)Il2CppGetMethodOffset(DLL_MAIN, NS_VAGE, "PunishPromptDuration", "get_isHpUnderPunishValue", 0))
            DobbyHook(p, (void*)hk_Punish, (void**)&orig_Punish);
    if (!orig_SkillSlot_LateUpdate)
        if (auto p = (void*)Il2CppGetMethodOffset(DLL_MAIN, NS_LOGIC, "SkillSlot", "LateUpdate", 1))
            DobbyHook(p, (void*)hk_SkillSlot_LateUpdate, (void**)&orig_SkillSlot_LateUpdate);
    if (!orig_SCI_LateUpdate)
        if (auto p = (void*)Il2CppGetMethodOffset(DLL_MAIN, NS_LOGIC, "SkillControlIndicator", "LateUpdate", 1))
            DobbyHook(p, (void*)hk_SCI_LateUpdate, (void**)&orig_SCI_LateUpdate);

    LOGI("[aim] hooks: pos=%p dir=%p skslot=%p upd=%p punish=%p",
         (void*)orig_GetUseSkillPosition, (void*)orig_GetUseSkillDirection,
         (void*)orig_SkillSlot_LateUpdate, (void*)orig_UpdateLogic, (void*)orig_Punish);
}
