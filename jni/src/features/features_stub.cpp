// features_stub.cpp — v2.6.0
// Features::tick() is a no-op in the new il2cpp hook-based system.
// All feature logic is now installed via DvlHook retry loop in main.cpp.
// This file only exists to satisfy the linker reference from esp.cpp.
#include "features.hpp"
#include "../../include/game_actors.hpp"

namespace Features {
void tick() {
    // In the new architecture, per-frame game logic runs inside il2cpp hooks
    // (ActorLinker.Update, SkillSlot.LateUpdate, etc.) — no polling needed.
    // Only invoke the MOBA diagnostic heartbeat here.
    modHeartbeat();
    selectAimTargets();
    ksttScan();
}
} // namespace Features
