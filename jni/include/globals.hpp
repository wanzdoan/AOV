#pragma once

#include "common.hpp"

#include <atomic>
#include <jni.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <mutex>
#include <set>

namespace Global {

extern JNIEnv* preEnv;
extern float uiScale;

struct UIRect {
    float x0 = 0.f;
    float y0 = 0.f;
    float x1 = 0.f;
    float y1 = 0.f;

    bool contains(float x, float y) const {
        return x1 > x0 && y1 > y0 && x >= x0 && x <= x1 &&
               y >= y0 && y <= y1;
    }
    bool empty() const { return !(x1 > x0 && y1 > y0); }
};

extern UIRect wmRect;
extern UIRect menuRect;
extern std::atomic<bool> touchCaptured;
extern std::atomic<int> realW;
extern std::atomic<int> realH;

extern uintptr_t libMainBase;
extern uintptr_t libUnityBase;

extern int screenWidth;
extern int screenHeight;
extern float screenCenterX;
extern float screenCenterY;

extern bool glReady;
extern GLuint glProgram;
extern GLuint vbo;

extern void* mainCamera;
extern Matrix4x4 viewProjMatrix;
extern std::mutex cameraMutex;

extern std::mutex entityMutex;
extern std::set<uintptr_t> heroSet;
extern uintptr_t localHero;

struct MapHackSettings {
    std::atomic_bool enabled{false};
    // These are ready when the master switch is first enabled.
    std::atomic_bool revealBushes{true};
    // Shader fog removal is independent from actor/minimap visibility and can
    // darken some map materials. Keep it opt-in.
    std::atomic_bool removeFog{false};
    std::atomic_bool showEnemyOnMinimap{true};
};
extern MapHackSettings mapHackSettings;

struct HeroEspSettings {
    std::atomic_bool enabled{false};
    std::atomic_bool showSkillCooldown{true};
    float espScale = 1.0f;
};
extern HeroEspSettings heroEspSettings;

struct ZoomSettings {
    std::atomic_bool enabled{false};
    std::atomic<float> multiplier{1.35f};
};
extern ZoomSettings zoomSettings;

struct TouchState {
    std::atomic_bool isDown{false};
    std::atomic<float> x{0.f};
    std::atomic<float> y{0.f};
};
extern TouchState touchState;

} // namespace Global
