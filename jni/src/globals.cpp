// =============================================================================
//  AOV Zygisk — Global State Definitions
// =============================================================================
#include "../include/globals.hpp"

namespace Global {

JNIEnv* preEnv  = nullptr;
float   uiScale = 1.0f;
UIRect  wmRect;
UIRect  menuRect;
std::atomic<bool> touchCaptured{false};
std::atomic<int> realW{0};
std::atomic<int> realH{0};

uintptr_t libMainBase  = 0;
uintptr_t libUnityBase = 0;

int   screenWidth   = 0;
int   screenHeight  = 0;
float screenCenterX = 0.f;
float screenCenterY = 0.f;

bool   glReady   = false;
GLuint glProgram = 0;
GLuint vbo       = 0;

void*      mainCamera    = nullptr;
Matrix4x4  viewProjMatrix = {};
std::mutex cameraMutex;

std::mutex          entityMutex;
std::set<uintptr_t> heroSet;
uintptr_t           localHero = 0;

// Feature settings (all default to disabled for safety)
MapHackSettings    mapHackSettings;
HeroEspSettings    heroEspSettings;
ZoomSettings       zoomSettings;
TouchState         touchState;

} // namespace Global
