#pragma once
// =============================================================================
//  game_actors.hpp — Actor tracking, game wrappers and target selection.
//  Port of Mod's Game.hpp. Provides:
//   - AimESPManager (enemy hero list + monster list)
//   - Actor accessor wrappers (get_position, get_objType, etc.)
//   - Target selection for aimbot (selectAimTargets)
//   - Auto Trừng Trị monster scan (ksttScan)
//   - Time Hồi Chiêu cooldown display (applyTimeHoiChieu via hook)
// =============================================================================

#include "include/il2cpp_resolver.hpp"
#include <cstdint>
#include <vector>

// Simple Vector types (used by this file)
#ifndef GAME_ACTORS_VECTOR_DEFINED
#define GAME_ACTORS_VECTOR_DEFINED
struct Vec3 { float x = 0, y = 0, z = 0; };
struct Vec2 { float x = 0, y = 0; };
inline float vec3Dist(const Vec3& a, const Vec3& b) {
    float dx = a.x-b.x, dy = a.y-b.y, dz = a.z-b.z;
    return __builtin_sqrtf(dx*dx + dy*dy + dz*dz);
}
inline Vec3 vec3Zero() { return {0,0,0}; }
inline bool vec3IsZero(const Vec3& v) { return v.x == 0 && v.y == 0 && v.z == 0; }
#endif

// ---- AimESPManager: actor list (heroes or monsters) -------------------------
struct AimESPManager {
    void* myPlayer = nullptr;
    std::vector<void*> enemies;
    void tryAddMyPlayer(void* a) { myPlayer = a; }
    void tryAddEnemy(void* a) {
        for (void* e : enemies) if (e == a) return;
        enemies.push_back(a);
    }
    void removeEnemyGivenObject(void* a) {
        for (size_t i = 0; i < enemies.size(); ++i)
            if (enemies[i] == a) { enemies.erase(enemies.begin() + i); break; }
        if (myPlayer == a) myPlayer = nullptr;
    }
};

extern AimESPManager* g_EspManager;      // enemy heroes
extern AimESPManager* g_MonsterManager;  // monsters (for ksttScan)
extern void*    g_myActor;               // local player ActorLinker
extern void*    g_actorKlass;            // ActorLinker Il2CppClass* (stale-ptr guard)
extern uint64_t g_myAccountUid;          // LobbyLogic.ulAccountUid

// Stale-pointer guard: a freed ActorLinker has klass != g_actorKlass.
bool liveActor(void* a);

// Resolve all method/field pointers (called once at startup).
void InitGameSymbols();

// Install ActorLinker.Update and DestroyActor hooks.
void InstallActorTrackingHooks();

// ---- Actor accessor wrappers ------------------------------------------------
Vec3  Actor_get_position(void* actor);
int   Actor_get_objType(void* actor);
bool  Actor_IsHostPlayer(void* actor);
int   Actor_get_objCamp(void* actor);
void* Actor_AsHero(void* actor);
void* Actor_ValueComponent(void* actor);
void* Actor_HudControl(void* actor);
int   Value_get_actorHp(void* vlc);
int   Value_get_actorHpTotal(void* vlc);
void* Get_actorManager();
Vec2  Camera_WorldToScreen(const Vec3& world);

// ---- Aim target buffers -----------------------------------------------------
// Buffer layout: [0..2]=my pos, [3..5]=enemy pos, [6..8]=enemy velocity (EMA),
//                [10]=lead multiplier (>0 = skill has lead), [11]=Rangeskill
extern float EnemyTarget [16];
extern float EnemyTarget1[16], EnemyTarget2[16], EnemyTarget3[16];

// Run per-frame target selection (fills EnemyTarget buffers + raises Willbp).
void selectAimTargets();
// Auto Trừng Trị: scan monsters, raise Willtt.
void ksttScan();
// Periodic heartbeat log (every 180 frames).
void modHeartbeat();
