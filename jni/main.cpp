// =============================================================================
//  AOV Zygisk — Main Entry Point (Zygisk Module)
//
//  Flow:
//    1. ZygiskNext load .so qua anonymous memfd injection
//    2. REGISTER_ZYGISK_MODULE(AOVModule)
//    3. onLoad() — lưu api + env
//    4. preAppSpecialize() — check app_data_dir hoặc nice_name
//    5. postAppSpecialize() — spawn hackThread (detached)
//    6. hackThread:
//       a. Poll libmain.so / libunity.so
//       b. Esp::init() → hook eglSwapBuffers + inject_event
//       c. Init_Il2cpp_Symbol() → bind il2cpp API (BL-histogram scanner)
//       d. Retry loop: EnsureApiHealthy + InstallAllFeatures mỗi 5s
//
//  Target: com.garena.game.kgvn (Liên Quân Mobile) 1.63.1.14
//  Version: v2.6.0
// =============================================================================
#include <sys/types.h>
#include "vendor/zygisk/include/zygisk.hpp"
#include "include/globals.hpp"
#include "src/hook/hook.hpp"
#include "src/esp/esp.hpp"
#include "src/diag/diag.hpp"
#include "src/anticheat/anticheat.hpp"
#include "include/il2cpp_resolver.hpp"
#include "include/game_actors.hpp"
#include "include/config_store.hpp"
#include <string>

#include <pthread.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <jni.h>

using namespace zygisk;

// ── Feature hook declarations -------------------------------------------------
void InitGameSymbols();
void InstallActorTrackingHooks();
void InstallMiscHooks();
void InstallFpsHooks();
void SkinMod_InstallHooks();
void InstallSkinApplyHooks();
void InitAimHooks();
void RefreshSkins();

// ── Install all il2cpp-based feature hooks (called in retry loop) -------------
static void InstallAllFeatures() {
    InitGameSymbols();           // resolve all method/field pointers (1.63.1.14)
    InstallMiscHooks();          // Hack Map V2, name reveal, camera zoom
    InstallFpsHooks();           // Unlock FPS 120
    SkinMod_InstallHooks();      // Unlock All Skins (14 hooks)
    InstallActorTrackingHooks(); // Actor tracking + Time Hồi Chiêu
    InitAimHooks();              // Aimbot + Auto Bộc Phá + Auto Trừng Trị
    InstallSkinApplyHooks();     // Kill-notify + Button skin
}

// ── Hack Thread ───────────────────────────────────────────────────────────────
static void* hackThread(void*) {
    LOGI("[main] hackThread start PID=%d — AOV v2.6.0", getpid());

    // Tầng 4: Đổi tên thread (direct syscall)
    AntiCheat::renameThreadDirect("UnityGfxDevice");

    Esp::ensureTouchHook();
    Esp::fetchRealDisplaySize();

    // Chờ libmain.so (tối đa 60s)
    uintptr_t mainBase = 0;
    for (int i = 0; i < 600 && mainBase == 0; i++) {
        usleep(100000);
        mainBase = Hook::mapsResolve(TARGET_LIB_MAIN);
    }
    if (!mainBase) { LOGE("[main] libmain.so timeout — exit"); return nullptr; }
    Global::libMainBase = mainBase;
    LOGI("[main] libmain.so @ 0x%lx", (unsigned long)mainBase);

    // Chờ libunity.so (tối đa 10s)
    uintptr_t unityBase = 0;
    for (int i = 0; i < 100 && unityBase == 0; i++) {
        usleep(100000);
        unityBase = Hook::mapsResolve(TARGET_LIB_UNITY);
    }
    if (unityBase) { Global::libUnityBase = unityBase; LOGI("[main] libunity.so @ 0x%lx", (unsigned long)unityBase); }
    else LOGW("[main] libunity.so not found — touch may not work");

    // Tầng 3: Maps hider
    AntiCheat::hideSelfFromMaps();

    Diag::run();

    // Init ESP (ImGui overlay)
    if (!Esp::init()) LOGE("[main] Esp::init() failed");

    // Chờ libil2cpp.so, bind il2cpp API
    LOGI("[main] waiting for libil2cpp.so...");
    for (int i = 0; i < 300; i++) {
        if (Hook::mapsResolve("libil2cpp.so")) { LOGI("[main] libil2cpp.so found"); break; }
        usleep(200000);
    }
    Init_Il2cpp_Symbol();  // BL-histogram scanner + runtime-ready gate

    // Retry loop (ported from Mod's DvlHook): re-installs hooks every 5/20s
    for (int pass = 0; ; pass++) {
        IL2Cpp::EnsureApiHealthy(3);
        InstallAllFeatures();
        if (pass == 0) LOGI("[main] ✓ feature hooks installed — v2.6.0 (1.63.1.14)");
        unsigned waitSec = (pass >= 24) ? 20u : 5u;
        for (unsigned rem = waitSec; (rem = sleep(rem)) > 0; ) {}
        if (pass > 1000000) break;
    }
    return nullptr;
}

// ── Zygisk Module Class ───────────────────────────────────────────────────────
class AOVModule : public ModuleBase {
public:
    void onLoad(Api* api, JNIEnv* env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(AppSpecializeArgs* args) override {
        if (!args) return;
        if (args->app_data_dir) {
            const char* dir = env->GetStringUTFChars(args->app_data_dir, nullptr);
            if (dir) {
                isTarget = (strstr(dir, TARGET_PKG) != nullptr);
                if(isTarget) appDataDirectory=dir;
                env->ReleaseStringUTFChars(args->app_data_dir, dir);
            }
        }
        if (!isTarget && args->nice_name) {
            const char* name = env->GetStringUTFChars(args->nice_name, nullptr);
            if (name) { isTarget = (strstr(name, TARGET_PKG) != nullptr); env->ReleaseStringUTFChars(args->nice_name, name); }
        }
        if (isTarget) {
            LOGI("[main] target process: %s", TARGET_PKG);
            Global::preEnv = env;
            // Tầng 2: Force unmount Magisk/KernelSU namespace
            api->setOption(zygisk::Option::FORCE_DENYLIST_UNMOUNT);
            // Tầng 5: Property spoofer (before tersafe loads)
            AntiCheat::installPropertyHook();
            Esp::patchJniTable(env);
        }
    }

    void postAppSpecialize(const AppSpecializeArgs*) override {
        if (!isTarget) return;
        // App UID and SELinux context now own this app-private path. Restore
        // before starting render/feature threads so mirrors agree from startup.
        ConfigStore::initialize(appDataDirectory.c_str());
        pthread_t tid;
        int ret = pthread_create(&tid, nullptr, hackThread, nullptr);
        if (ret != 0) { LOGE("[main] pthread_create failed: %s", strerror(ret)); return; }
        pthread_detach(tid);
        LOGI("[main] hackThread spawned");
    }

private:
    Api*    api      = nullptr;
    JNIEnv* env      = nullptr;
    bool    isTarget = false;
    std::string appDataDirectory;
};

REGISTER_ZYGISK_MODULE(AOVModule)
