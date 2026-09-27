// =============================================================================
//  game.cpp — Game::namespace bridge, v2.6.0
//  Wraps the new il2cpp-based actor system (game_actors.cpp) to provide the
//  interface expected by esp.cpp and menu.cpp (heroCount, localInfo, etc.).
//  The old hardcoded offset system has been REPLACED by the by-name il2cpp
//  resolver (il2cpp_resolver.cpp + game_actors.cpp).
// =============================================================================

#include "game.hpp"
#include "../../include/globals.hpp"
#include "../../include/game_actors.hpp"
#include "../hook/hook.hpp"

#include <algorithm>
#include <vector>
#include <mutex>
#include <cstring>
#include <cstdio>

namespace Game {

// ── Public API ────────────────────────────────────────────────────────────────

void tick() {
    // No-op in the new il2cpp hook-based system.
    // Actor state is updated via InstallActorTrackingHooks() (ActorLinker.Update hook).
    // Called every frame from esp.cpp render loop — kept for API compatibility.
}

int heroCount() {
    if (!g_EspManager) return 0;
    int count = 0;
    if (g_myActor) count = 1;       // local player
    count += (int)g_EspManager->enemies.size();
    return count;
}

bool localInfo(char* nameOut, int* hp, int* maxHp) {
    if (!liveActor(g_myActor)) return false;
    void* v = Actor_ValueComponent(g_myActor);
    if (!v) return false;
    int h = Value_get_actorHp(v);
    int m = Value_get_actorHpTotal(v);
    if (m <= 0) return false;
    if (nameOut) snprintf(nameOut, 48, "LocalHero");
    if (hp)    *hp    = h;
    if (maxHp) *maxHp = m;
    return true;
}

uintptr_t il2cppBase() {
    return Hook::mapsResolve("libil2cpp.so");
}

void* actorManager() {
    return Get_actorManager();
}

// heroes() — returns a minimal HeroInfo list from the il2cpp actor tracking.
// NOTE: The old 1.63.1.10 hardcoded-offset struct fills are gone.
// esp.cpp uses heroes() for ESP rendering. We fill position + HP from wrappers.
static std::vector<HeroInfo> s_heroList;
static std::mutex s_heroMutex;

const std::vector<HeroInfo>& heroes() {
    std::lock_guard<std::mutex> lock(s_heroMutex);
    s_heroList.clear();

    auto fill = [&](void* actor, bool isLocal, bool isEnemy) {
        if (!liveActor(actor)) return;
        HeroInfo hi;
        hi.linker  = (uintptr_t)actor;
        hi.isLocal = isLocal;
        hi.isEnemy = isEnemy;
        void* v = Actor_ValueComponent(actor);
        hi.hp    = Value_get_actorHp(v);
        hi.maxHp = Value_get_actorHpTotal(v);
        if (hi.maxHp <= 0) return;
        Vec3 pos = Actor_get_position(actor);
        hi.x = pos.x; hi.y = pos.y; hi.z = pos.z;
        // Project to screen
        Vec2 sc = Camera_WorldToScreen(pos);
        if (Global::screenWidth > 0 && Global::screenHeight > 0 &&
            sc.x > 0 && sc.x < Global::screenWidth &&
            sc.y > 0 && sc.y < Global::screenHeight) {
            hi.sx = sc.x; hi.sy = sc.y; hi.onScreen = true;
        }
        // cd[] is filled by the in-game hook (applyTimeHoiChieu) — leave as -1 default
        s_heroList.push_back(hi);
    };

    // Local player
    if (liveActor(g_myActor)) fill(g_myActor, true, false);
    // Enemy heroes
    if (g_EspManager) {
        for (void* e : g_EspManager->enemies) fill(e, false, true);
    }
    return s_heroList;
}

} // namespace Game
