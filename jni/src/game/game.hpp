#pragma once

#include <cstdint>
#include <vector>

namespace Game {

struct HeroInfo {
    uintptr_t linker = 0;
    char name[48] = {};
    int hp = 0;
    int maxHp = 0;
    int level = 0;
    // Remaining cooldown in seconds. -1 means the slot was unavailable.
    int cd[4] = {-1, -1, -1, -1};
    int cdMax[4] = {0, 0, 0, 0};
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
    float sx = 0.f;
    float sy = 0.f;
    bool onScreen = false;
    bool isLocal = false;
    bool isEnemy = false;
};

void tick();
int heroCount();
bool localInfo(char* nameOut, int* hp, int* maxHp);
uintptr_t il2cppBase();
void* actorManager();
const std::vector<HeroInfo>& heroes();

} // namespace Game
