#include "features.hpp"

#include "../game/game.hpp"
#include "../hook/hook.hpp"
#include "../../include/game_offsets.hpp"
#include "../../include/globals.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>

namespace Features {
namespace GO = GameOffsets;

template <typename T>
static T readU(uintptr_t address) {
    if (!isValidPtr(address) ||
        !isValidPtr(address + sizeof(T) - 1)) return T{};
    T value{};
    memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T));
    return value;
}

template <typename T>
static void writeU(uintptr_t address, const T& value) {
    if (!isValidPtr(address) ||
        !isValidPtr(address + sizeof(T) - 1)) return;
    memcpy(reinterpret_cast<void*>(address), &value, sizeof(T));
}

static bool s_fogDisabled = false;
static bool s_fogOriginalKnown = false;
static bool s_fogOriginallyEnabled = false;
static uintptr_t s_lastFogObject = 0;

static void applyFog() {
    uintptr_t base = Game::il2cppBase();
    if (!base) return;

    using FnGetFowManager = void* (*)();
    auto getFowManager = reinterpret_cast<FnGetFowManager>(
        base + GO::Kyrios_get_fowMgr);
    uintptr_t manager = reinterpret_cast<uintptr_t>(getFowManager());
    uintptr_t fog = isValidPtr(manager)
        ? readU<uintptr_t>(manager + GO::FowMgr_fow)
        : 0;
    if (!isValidPtr(fog)) return;

    // Every match gets a new FogOfWar object. Do not carry a guessed shader
    // state from the lobby or a previous match into it.
    if (fog != s_lastFogObject) {
        s_lastFogObject = fog;
        s_fogDisabled = false;
        s_fogOriginalKnown = false;
    }

    const bool wantDisabled = Global::mapHackSettings.enabled.load() &&
                              Global::mapHackSettings.removeFog.load();
    if (wantDisabled == s_fogDisabled) return;

    if (wantDisabled) {
        s_fogOriginallyEnabled =
            readU<uint8_t>(fog + GO::Fog_enable) != 0;
        s_fogOriginalKnown = true;
        uintptr_t address = base + GO::Fog_DisableShader;
        if (!Hook::isExecutable(address)) {
            LOGE("[feat] fog disable RVA is not executable");
            return;
        }
        reinterpret_cast<void (*)()>(address)();
        s_fogDisabled = true;
        LOGI("[feat] fog shader disabled (originalEnabled=%d)",
             s_fogOriginallyEnabled ? 1 : 0);
        return;
    }

    // The old build always called EnableShaderFogFunction on OFF. On devices
    // where the game had the keyword disabled to begin with, that created the
    // reported dark-map regression. Restore only a state we actually observed.
    if (s_fogOriginalKnown && s_fogOriginallyEnabled) {
        uintptr_t address = base + GO::Fog_EnableShader;
        if (!Hook::isExecutable(address)) {
            LOGE("[feat] fog restore RVA is not executable");
            return;
        }
        reinterpret_cast<void (*)()>(address)();
        LOGI("[feat] fog shader restored to original enabled state");
    } else {
        LOGI("[feat] fog restore skipped (original shader was disabled)");
    }
    s_fogDisabled = false;
    s_fogOriginalKnown = false;
}

using FnMobaUpdate = void (*)(void*);
static FnMobaUpdate s_originalMobaUpdate = nullptr;
static bool s_zoomHookAttempted = false;
static bool s_zoomHooked = false;
static std::atomic<uint64_t> s_zoomHookCalls{0};
static uintptr_t s_zoomObject = 0;
static uintptr_t s_zoomSettingsObject = 0;
static float s_originalZoom = 0.f;
static float s_originalMaxZoom = 0.f;
static bool s_zoomApplied = false;

static void hookMobaUpdate(void* object) {
    uint64_t call = s_zoomHookCalls.fetch_add(1, std::memory_order_relaxed) + 1;
    uintptr_t camera = reinterpret_cast<uintptr_t>(object);
    bool enabled = Global::zoomSettings.enabled.load();

    if (isValidPtr(camera)) {
        uintptr_t settings = readU<uintptr_t>(
            camera + GO::MobaCamera_settings);
        uintptr_t zoomSettings = isValidPtr(settings)
            ? readU<uintptr_t>(settings + GO::MobaSettings_zoom)
            : 0;
        float currentZoom = readU<float>(
            camera + GO::MobaCamera_currentZoom);

        if (enabled && std::isfinite(currentZoom) && currentZoom > 0.01f &&
            isValidPtr(zoomSettings)) {
            if (!s_zoomApplied || s_zoomObject != camera ||
                s_zoomSettingsObject != zoomSettings) {
                s_zoomObject = camera;
                s_zoomSettingsObject = zoomSettings;
                s_originalZoom = currentZoom;
                s_originalMaxZoom = readU<float>(
                    zoomSettings + GO::MobaZoom_max);
                s_zoomApplied = true;
                LOGI("[feat] zoom baseline: camera=%p amount=%.3f max=%.3f",
                     object, s_originalZoom, s_originalMaxZoom);
            }

            float multiplier = std::clamp(
                Global::zoomSettings.multiplier.load(), 1.0f, 2.5f);
            float target = s_originalZoom * multiplier;
            float extendedMax = std::max(s_originalMaxZoom, target + 0.01f);
            writeU<float>(zoomSettings + GO::MobaZoom_max, extendedMax);
            writeU<float>(camera + GO::MobaCamera_currentZoom, target);
            writeU<uint8_t>(camera + GO::MobaCamera_changeInCamera, 1);

            if (call == 1 || (call % 600u) == 0u) {
                LOGI("[feat] zoom active: calls=%llu target=%.3f max=%.3f "
                     "multiplier=%.2f",
                     static_cast<unsigned long long>(call), target,
                     extendedMax, multiplier);
            }
        } else if (!enabled && s_zoomApplied && s_zoomObject == camera) {
            if (isValidPtr(s_zoomSettingsObject)) {
                writeU<float>(s_zoomSettingsObject + GO::MobaZoom_max,
                              s_originalMaxZoom);
            }
            writeU<float>(camera + GO::MobaCamera_currentZoom,
                          s_originalZoom);
            writeU<uint8_t>(camera + GO::MobaCamera_changeInCamera, 1);
            LOGI("[feat] zoom restored: amount=%.3f max=%.3f calls=%llu",
                 s_originalZoom, s_originalMaxZoom,
                 static_cast<unsigned long long>(call));
            s_zoomApplied = false;
        }
    }

    if (s_originalMobaUpdate) s_originalMobaUpdate(object);
}

static void installZoomHook() {
    if (s_zoomHookAttempted) return;
    uintptr_t base = Game::il2cppBase();
    if (!base) return;

    s_zoomHookAttempted = true;
    s_zoomHooked = Hook::hookGameRva(
        "Moba_Camera.Update", base, GO::MobaCamera_Update,
        reinterpret_cast<void*>(hookMobaUpdate),
        reinterpret_cast<void**>(&s_originalMobaUpdate));
    LOGI("[feat] zoom hook: %s", s_zoomHooked ? "ready" : "failed");
}

void tick() {
    applyFog();
    installZoomHook();
}

} // namespace Features
