// =============================================================================
//  game_actors.cpp â€” Actor tracking hooks, game wrappers, Time Há»“i ChiÃªu,
//  target selection, Auto Trá»«ng Trá»‹.
//  Port of Mod's Game.cpp, faithful to original logic.
// =============================================================================

#include "include/game_actors.hpp"
#include "include/aov_config.hpp"
#include "include/il2cpp_resolver.hpp"
#include "include/common.hpp"
#include "dobby.h"

#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>
#include <time.h>

// ---- Cached game method pointers --------------------------------------------
static Vec3  (*m_get_position)(void*)    = nullptr;
static int   (*m_get_objType)(void*)     = nullptr;
static bool  (*m_IsHostPlayer)(void*)    = nullptr;
static int   (*m_get_objCamp)(void*)     = nullptr;
static void* (*m_AsHero)(void*)          = nullptr;
static int   (*m_get_actorHp)(void*)     = nullptr;
static int   (*m_get_actorHpTotal)(void*)= nullptr;
static void* (*m_get_actorManager)()     = nullptr;
static void* (*m_get_main)()             = nullptr;  // Camera.get_main
static Vec3  (*m_WorldToScreenPoint)(void*, Vec3) = nullptr;
// HudComponent3D.SetPlayerName(hud, str1_white, str2_yellow, flags, str0)
// str2 (yellow) = leftmost, str1 (white) = rightmost
static void* (*m_SetPlayerName)(void*, void*, void*, int, void*) = nullptr;
// il2cpp_string_new: managed string from C string (recovered by scanner)
static void* (*m_il2cpp_string_new)(const char*) = nullptr;

// ---- Cached field offsets ---------------------------------------------------
static uintptr_t f_valueComp = 0, f_hudControl = 0;
static uintptr_t f_hsd1 = 0, f_hsd2 = 0, f_hsd3 = 0, f_hsd5 = 0; // heroWrapSkillData_{1,2,3,5}
static uintptr_t f_hudType = 0, f_hudHeight = 0;  // for ksttScan
static uintptr_t f_level = 0;                      // actorSoulLevel for buff threshold

// ---- Tracking hook trampolines ----------------------------------------------
static void (*orig_ActorUpdate)(void*)   = nullptr;
static void (*orig_DestroyActor)(void*)  = nullptr;

// ---- Globals ----------------------------------------------------------------
AimESPManager* g_EspManager     = new AimESPManager();
AimESPManager* g_MonsterManager = new AimESPManager();
void*    g_myActor     = nullptr;
void*    g_actorKlass  = nullptr;
uint64_t g_myAccountUid = 0;

float EnemyTarget [16] = {};
float EnemyTarget1[16] = {}, EnemyTarget2[16] = {}, EnemyTarget3[16] = {};

// ---- Stale-pointer guard ----------------------------------------------------
bool liveActor(void* a) {
    uintptr_t p = (uintptr_t)a;
    if (!a || !g_actorKlass || (p & 7) || p < 0x10000000ULL || p >= 0x0000800000000000ULL) return false;
    return *(void**)a == g_actorKlass;
}

// ============================================================
//  InitGameSymbols â€” resolve all method/field pointers once
// ============================================================
void InitGameSymbols() {
    using namespace IL2Cpp;
    m_get_position      = (Vec3(*)(void*))   Il2CppGetMethodOffset(DLL_MAIN, NS_ACTOR, "ActorLinker", "get_position", 0);
    m_get_objType       = (int(*)(void*))    Il2CppGetMethodOffset(DLL_MAIN, NS_ACTOR, "ActorLinker", "get_objType", 0);
    m_IsHostPlayer      = (bool(*)(void*))   Il2CppGetMethodOffset(DLL_MAIN, NS_ACTOR, "ActorLinker", "IsHostPlayer", 0);
    m_get_objCamp       = (int(*)(void*))    Il2CppGetMethodOffset(DLL_MAIN, NS_ACTOR, "ActorLinker", "get_objCamp", 0);
    m_AsHero            = (void*(*)(void*))  Il2CppGetMethodOffset(DLL_MAIN, NS_ACTOR, "ActorLinker", "AsHero", 0);
    m_get_actorHp       = (int(*)(void*))    Il2CppGetMethodOffset(DLL_MAIN, NS_ACTOR, "ValueLinkerComponent", "get_actorHp", 0);
    m_get_actorHpTotal  = (int(*)(void*))    Il2CppGetMethodOffset(DLL_MAIN, NS_ACTOR, "ValueLinkerComponent", "get_actorHpTotal", 0);
    m_get_actorManager  = (void*(*)())       Il2CppGetMethodOffset(DLL_MAIN, "Kyrios", "KyriosFramework", "get_actorManager", 0);
    m_SetPlayerName     = (void*(*)(void*,void*,void*,int,void*)) Il2CppGetMethodOffset(DLL_MAIN, NS_LOGIC, "HudComponent3D", "SetPlayerName", 4);
    m_get_main          = (void*(*)())       Il2CppGetMethodOffset("UnityEngine.CoreModule.dll", "UnityEngine", "Camera", "get_main", 0);
    m_WorldToScreenPoint= (Vec3(*)(void*,Vec3)) Il2CppGetMethodOffset("UnityEngine.CoreModule.dll", "UnityEngine", "Camera", "WorldToScreenPoint", 1);

    // ActorLinker fields (Kyrios.Actor namespace)
    f_valueComp  = Il2CppGetFieldOffset(DLL_MAIN, NS_ACTOR, "ActorLinker", "ValueComponent");
    f_hudControl = Il2CppGetFieldOffset(DLL_MAIN, NS_ACTOR, "ActorLinker", "HudControl");
    f_hsd1       = Il2CppGetFieldOffset(DLL_MAIN, NS_ACTOR, "HeroWrapperData", "heroWrapSkillData_1");
    f_hsd2       = Il2CppGetFieldOffset(DLL_MAIN, NS_ACTOR, "HeroWrapperData", "heroWrapSkillData_2");
    f_hsd3       = Il2CppGetFieldOffset(DLL_MAIN, NS_ACTOR, "HeroWrapperData", "heroWrapSkillData_3");
    f_hsd5       = Il2CppGetFieldOffset(DLL_MAIN, NS_ACTOR, "HeroWrapperData", "heroWrapSkillData_5");
    // HudComponent3D fields (Assets.Scripts.GameLogic namespace)
    f_hudType    = Il2CppGetFieldOffset(DLL_MAIN, NS_LOGIC, "HudComponent3D", "HudType");
    f_hudHeight  = Il2CppGetFieldOffset(DLL_MAIN, NS_LOGIC, "HudComponent3D", "hudHeight");
    // actorSoulLevel (for Trá»«ng Trá»‹ buff threshold calculation)
    f_level      = Il2CppGetFieldOffset(DLL_MAIN, NS_ACTOR, "ValueLinkerComponent", "<actorSoulLevel>k__BackingField");

    // il2cpp_string_new is stripped from exports â†’ get from scanner
    m_il2cpp_string_new = (void*(*)(const char*)) il2cppExports::il2cpp_string_new;

    LOGI("[game] symbols: pos=%p objType=%p hp=%p setName=%p", (void*)m_get_position, (void*)m_get_objType, (void*)m_get_actorHp, (void*)m_SetPlayerName);
    LOGI("[game] fields: valueComp=0x%zx hudCtrl=0x%zx hsd1=0x%zx hudType=0x%zx", f_valueComp, f_hudControl, f_hsd1, f_hudType);
}

// ============================================================
//  Actor accessor wrappers
// ============================================================
Vec3  Actor_get_position(void* a)    { return (a && m_get_position) ? m_get_position(a) : vec3Zero(); }
int   Actor_get_objType(void* a)     { return (a && m_get_objType)  ? m_get_objType(a)  : -1; }
bool  Actor_IsHostPlayer(void* a)    { return (a && m_IsHostPlayer) ? m_IsHostPlayer(a) : false; }
int   Actor_get_objCamp(void* a)     { return (a && m_get_objCamp)  ? m_get_objCamp(a)  : -1; }
void* Actor_AsHero(void* a)          { return (a && m_AsHero)       ? m_AsHero(a)       : nullptr; }
void* Actor_ValueComponent(void* a)  { return (a && f_valueComp)    ? *(void**)((char*)a + f_valueComp) : nullptr; }
void* Actor_HudControl(void* a)      { return (a && f_hudControl)   ? *(void**)((char*)a + f_hudControl) : nullptr; }
int   Value_get_actorHp(void* v)     { return (v && m_get_actorHp)  ? m_get_actorHp(v)  : 0; }
int   Value_get_actorHpTotal(void* v){ return (v && m_get_actorHpTotal) ? m_get_actorHpTotal(v) : 0; }
void* Get_actorManager()             { return m_get_actorManager ? m_get_actorManager() : nullptr; }

Vec2 Camera_WorldToScreen(const Vec3& w) {
    if (!m_get_main || !m_WorldToScreenPoint) return {};
    void* cam = m_get_main(); if (!cam) return {};
    Vec3 s = m_WorldToScreenPoint(cam, {w.x, w.y, w.z});
    return {s.x, s.y};
}

// ============================================================
//  Time Há»“i ChiÃªu â€” skill cooldown overlay via HUD name label
//  SetPlayerName(hud, str1_white, str2_yellow, flags, str0):
//    str2 (YELLOW) = leftmost = support spell (phá»¥ trá»£) cooldown
//    str1 (WHITE)  = rightmost = skill 1/2/3 cooldowns
// ============================================================
static void applyTimeHoiChieu(void* actor) {
    if (!(cfg.timeHoiChieu && m_AsHero && m_SetPlayerName && m_il2cpp_string_new)) return;
    void* hero = m_AsHero(actor);
    void* hud  = Actor_HudControl(actor);
    if (!hero || !hud) return;

    // ms â†’ seconds (ceil, matching the game's own HUD rounding)
    auto cd = [&](uintptr_t f) -> int {
        if (!f) return 0;
        int ms = *(int*)((char*)hero + f + 0x1c);
        return ms > 0 ? (ms + 999) / 1000 : 0;
    };

    // Format: [phá»¥ trá»£_cd] [chiÃªu1_cd] [chiÃªu2_cd] [chiÃªu3_cd]
    // phá»¥ trá»£ (slot 5) â†’ YELLOW (str2, left)
    // chiÃªu 1/2/3 (slots 1/2/3) â†’ WHITE (str1, right)
    char sup[24], sk[48];
    snprintf(sup, sizeof(sup), "[%d]", cd(f_hsd5));
    snprintf(sk,  sizeof(sk),  " [%d] [%d] [%d]", cd(f_hsd1), cd(f_hsd2), cd(f_hsd3));

    void* sSup = m_il2cpp_string_new(sup);  // YELLOW â€” phá»¥ trá»£ (left)
    void* sSk  = m_il2cpp_string_new(sk);   // WHITE  â€” 3 chiÃªu (right)
    void* s0   = m_il2cpp_string_new("");
    m_SetPlayerName(hud, sSk, sSup, 1, s0);
}

// ============================================================
//  ActorLinker.Update hook â€” track actors + apply THC
// ============================================================
static void hk_ActorUpdate(void* actor) {
    if (!actor) return;
    if (orig_ActorUpdate) orig_ActorUpdate(actor);
    if (!g_actorKlass) g_actorKlass = *(void**)actor;  // capture klass pointer once
    if (!m_get_objType || !m_IsHostPlayer || !m_get_objCamp) return;

    int type = m_get_objType(actor);
    if (type == 1) {                           // minion/monster
        g_MonsterManager->tryAddEnemy(actor);
    } else if (type == 0) {                    // hero
        if (m_IsHostPlayer(actor)) {
            g_EspManager->tryAddMyPlayer(actor);
            g_myActor = actor;
        } else if (g_EspManager->myPlayer && liveActor(g_myActor) &&
                   m_get_objCamp(g_myActor) != m_get_objCamp(actor)) {
            g_EspManager->tryAddEnemy(actor);  // enemy hero
        }
    }
    applyTimeHoiChieu(actor);
}

// ============================================================
//  ActorLinker.DestroyActor hook â€” remove from tracking lists
// ============================================================
static std::unordered_map<void*, Vec3> g_velLast;  // last position
static std::unordered_map<void*, Vec3> g_velVal;   // velocity EMA

static void hk_DestroyActor(void* actor) {
    if (!actor) return;
    if (orig_DestroyActor) orig_DestroyActor(actor);
    g_EspManager->removeEnemyGivenObject(actor);
    g_MonsterManager->removeEnemyGivenObject(actor);
    g_velLast.erase(actor);
    g_velVal.erase(actor);
    if (g_myActor == actor) g_myActor = nullptr;
}

// ============================================================
//  InstallActorTrackingHooks
// ============================================================
void InstallActorTrackingHooks() {
    if (!orig_ActorUpdate)
        if (auto p = (void*)Il2CppGetMethodOffset(DLL_MAIN, NS_ACTOR, "ActorLinker", "Update", 0))
            DobbyHook(p, (void*)hk_ActorUpdate, (void**)&orig_ActorUpdate);
    if (!orig_DestroyActor)
        if (auto p = (void*)Il2CppGetMethodOffset(DLL_MAIN, NS_ACTOR, "ActorLinker", "DestroyActor", 0))
            DobbyHook(p, (void*)hk_DestroyActor, (void**)&orig_DestroyActor);
    LOGI("[game] actor tracking hooks: Update=%p DestroyActor=%p", (void*)orig_ActorUpdate, (void*)orig_DestroyActor);
}

// ============================================================
//  Enemy velocity EMA tracker (EMA Î±=0.35)
// ============================================================
static Vec3 updateVel(void* e, Vec3 p, float dt) {
    if (dt > 0.001f && dt <= 0.2f) {
        auto& last = g_velLast[e];
        auto& vel  = g_velVal[e];
        Vec3 disp = {p.x - last.x, p.y - last.y, p.z - last.z};
        vel = {vel.x * 0.65f + disp.x * 0.35f,
               vel.y * 0.65f + disp.y * 0.35f,
               vel.z * 0.65f + disp.z * 0.35f};
        last = p;
    }
    auto it = g_velVal.find(e);
    return it != g_velVal.end() ? it->second : vec3Zero();
}

// ============================================================
//  selectAimTargets â€” fills EnemyTarget{1,2,3} for aimbot
//  Also raises Willbp (Auto Bá»™c PhÃ¡) when conditions met.
// ============================================================
void selectAimTargets() {
    if (!liveActor(g_myActor)) return;
    // DeltaTime: approximate from monotonic clock (imgui not always available here)
    static int64_t s_lastUs = 0;
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    int64_t nowUs = (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    float dt = s_lastUs ? (float)(nowUs - s_lastUs) / 1000000.0f : 0.016f;
    if (dt < 0.001f || dt > 0.5f) dt = 0.016f;
    s_lastUs = nowUs;

    Vec3 me = Actor_get_position(g_myActor);

    struct Slot { float* T; bool on; float range; };
    Slot sl[3] = {
        {EnemyTarget1, AimSkill1, Rangeskill1},
        {EnemyTarget2, AimSkill2, Rangeskill2},
        {EnemyTarget3, AimSkill3, Rangeskill3},
    };
    float score[3] = {1e30f, 1e30f, 1e30f};
    for (auto& s : sl) {
        s.T[0]=me.x; s.T[1]=me.y; s.T[2]=me.z;
        s.T[10]=0.5f; s.T[11]=s.range;
        s.T[3]=s.T[4]=s.T[5]=s.T[6]=s.T[7]=s.T[8]=0.0f;
    }

    float pvi = PviChieu > 0.0f ? PviChieu : 25.0f;
    void* mainBest = nullptr; float mainScore = 1e30f;

    for (void* e : g_EspManager->enemies) {
        if (!liveActor(e) || e == g_myActor) continue;
        if (Actor_get_objType(e) != 0) continue;
        if (g_myActor && m_get_objCamp && m_get_objCamp(g_myActor) == m_get_objCamp(e)) continue;
        void* v = Actor_ValueComponent(e);
        int hp = Value_get_actorHp(v);
        if (hp <= 0) continue;
        int tot = Value_get_actorHpTotal(v);
        float hpPct = tot ? (float)hp / tot * 100.0f : (float)hp;
        Vec3 ep = Actor_get_position(e);
        float d = vec3Dist(me, ep);
        Vec3 vel = updateVel(e, ep, dt);

        // Auto Bá»™c PhÃ¡: enemy within 5m, HP â‰¤ 15% of missing HP
        if (Ksbp && d < 5.0f && hp > 1 && (float)hp <= (float)(tot - hp) * 0.15f) Willbp = true;

        float sc = (aimType == AIM_LOW_HP_PCT) ? hpPct
                 : (aimType == AIM_LOW_HP)     ? (float)hp
                 :                               d;

        if (AimSkill) for (int i = 0; i < 3; i++) {
            if (!sl[i].on) continue;
            if (sl[i].range > 0.0f && d > sl[i].range) continue;
            if (sc < score[i]) {
                score[i] = sc;
                sl[i].T[3]=ep.x; sl[i].T[4]=ep.y; sl[i].T[5]=ep.z;
                sl[i].T[6]=vel.x; sl[i].T[7]=vel.y; sl[i].T[8]=vel.z;
            }
        }
        if (d < pvi && sc < mainScore) { mainScore = sc; mainBest = e; }
    }

    EnemyTarget[0]=me.x; EnemyTarget[1]=me.y; EnemyTarget[2]=me.z;
    if (mainBest) { Vec3 tp = Actor_get_position(mainBest); EnemyTarget[3]=tp.x; EnemyTarget[4]=tp.y; EnemyTarget[5]=tp.z; }
    else { EnemyTarget[3]=EnemyTarget[4]=EnemyTarget[5]=0.0f; }
}

// ============================================================
//  ksttScan â€” Auto Trá»«ng Trá»‹: scan monsters, raise Willtt
// ============================================================
void ksttScan() {
    if (!Kstt || !liveActor(g_myActor) || !f_hudControl || !f_hudType || !f_hudHeight) return;
    Vec3 me = Actor_get_position(g_myActor);
    void* myV = Actor_ValueComponent(g_myActor);
    int myLevel = (myV && f_level) ? *(int*)((char*)myV + f_level) : 0;
    static std::unordered_map<int,char> logged;

    void* best = nullptr; float bestD = 4.0f; int bType = 0, bId = 0, bHp = 0;
    for (void* m : g_MonsterManager->enemies) {
        if (!liveActor(m)) continue;
        void* v = Actor_ValueComponent(m);
        int hp = Value_get_actorHp(v);
        if (hp <= 0) continue;
        void* hud = *(void**)((char*)m + f_hudControl);
        if (!hud) continue;
        int hudType = *(int*)((char*)hud + f_hudType);
        int hudId   = *(int*)((char*)hud + f_hudHeight);
        Vec3 mp = Actor_get_position(m);
        float d = vec3Dist(me, mp);
        int tot = Value_get_actorHpTotal(v);
        // Discovery log â€” once per type+id combination
        if (d < 22.0f && !logged.count((hudType<<20)^hudId)) {
            logged[(hudType<<20)^hudId] = 1;
            LOGI("[kstt] monster type=%d id=%d(0x%x) hp=%d/%d d=%.1f", hudType, hudId, hudId, hp, tot, d);
        }
        if (d < bestD) { bestD = d; best = m; bType = hudType; bId = hudId; bHp = hp; }
    }

    // Epic (TÃ  Tháº§n/Rá»“ng) = HudType 4; gate on Killm (is executable by Punish)
    bool isEpic = best && (bType == 4);
    // Buff (BÃ¹a xanh/Ä‘á») = HudType 1, specific ids; use HP threshold instead of Killm
    bool isBuff = best && (bType == 1 && (bId == 0xb54 || bId == 0xcb2));
    if (isEpic && g_TaThanRong && Killm) {
        Willtt = true; LOGI("[kstt] WILLTT epic id=%d", bId);
    } else if (isBuff && g_BuaXanhDo) {
        int thr = myLevel * 100 + 1250;
        if (bHp <= thr) { Willtt = true; LOGI("[kstt] WILLTT buff id=0x%x hp=%d thr=%d", bId, bHp, thr); }
    }
}

// ============================================================
//  Heartbeat diagnostic log
// ============================================================
void modHeartbeat() {
    static int n = 0;
    if ((n++ % 180) != 0) return;
    LOGI("[hb] Kstt=%d Ksbp=%d Aim=%d Elsu=%d | myActor=%p mon=%zu esp=%zu | hud=0x%zx hudT=0x%zx",
         (int)Kstt, (int)Ksbp, (int)AimSkill, (int)EspElsu,
         g_myActor, g_MonsterManager->enemies.size(), g_EspManager->enemies.size(),
         f_hudControl, f_hudType);
}
