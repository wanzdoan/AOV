// =============================================================================
//  AOV Zygisk — MOBA ESP Rendering + eglSwapBuffers Hook
//
//  MOBA-specific overlays (NOT FPS):
//    - HP/Mana bars above hero heads
//    - Hero name + level labels
//    - Highlight circle under enemy/ally heroes
//    - Skill cooldown display (Q/W/E/R)
//    - Tower attack-range circles
//
//  Pipeline:
//    Hook eglSwapBuffers → ImGui init → per-frame:
//      1. Menu::render() (UI widgets via ImGui window)
//      2. renderHeroEsp() (via ImGui BackgroundDrawList)
//      3. renderTowerRange()
//      4. original eglSwapBuffers()
//
//  Touch: inject_event hook in libunity.so routes to ImGui_ImplAndroid_HandleInputEvent
// =============================================================================
#include "esp.hpp"
#include "../hook/hook.hpp"
#include "../menu/menu.hpp"
#include "../features/features.hpp"
#include "../game/game.hpp"
#include "../../include/globals.hpp"
#include "../../include/offsets.hpp"

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <dlfcn.h>
#include <jni.h>
#include <cmath>
#include <atomic>
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
#include <sys/mman.h>

#include "../../vendor/imgui/imgui.h"
#include "../../vendor/imgui/backends/imgui_impl_opengl3.h"
#include "../../vendor/imgui/backends/imgui_impl_android.h"

// ── EGL ───────────────────────────────────────────────────────────────────────
using FnEglSwapBuffers  = EGLBoolean (*)(EGLDisplay, EGLSurface);
using FnEglQuerySurface = EGLBoolean (*)(EGLDisplay, EGLSurface, EGLint, EGLint*);
using FnInjectEvent     = int        (*)(JNIEnv*, jobject, jobject);

static FnEglSwapBuffers  orig_eglSwapBuffers = nullptr;
static FnEglQuerySurface fn_eglQuerySurface  = nullptr;
static FnInjectEvent     orig_injectEvent    = nullptr;

// ── ImGui state (declared early — hook guards on this) ────────────────────────
static bool s_imguiReady = false;

// ── nativeInjectEvent hook (Unity 2022.3 touch funnel) ───────────────────────
// Found via static analysis of AOV's libunity.so (Unity 2022.3.5f1):
//   native method "nativeInjectEvent" sig "(Landroid/view/InputEvent;)Z"
//   registered on class com/unity3d/player/UnityPlayer.
// Installed via Zygisk api->hookJniNativeMethods (no byte pattern needed).
using FnNativeInjectEvent = jboolean (*)(JNIEnv*, jobject, jobject);
static FnNativeInjectEvent  orig_nativeInjectEvent = nullptr;
static std::atomic<int>     s_injectLogCount{0};

static jboolean hook_nativeInjectEvent(JNIEnv* env, jobject thiz,
                                       jobject inputEvent) {
    auto callOrig = [&]() -> jboolean {
        if (orig_nativeInjectEvent)
            return orig_nativeInjectEvent(env, thiz, inputEvent);
        return JNI_FALSE;
    };
    if (!s_imguiReady || !inputEvent || !env) return callOrig();

    if (s_injectLogCount.load() < 3) {
        LOGI("[esp] nativeInjectEvent call #%d",
             s_injectLogCount.fetch_add(1) + 1);
    }

    jclass motionClass = env->FindClass("android/view/MotionEvent");
    if (motionClass && env->IsInstanceOf(inputEvent, motionClass)) {
        // Cache methodID (chỉ lookup 1 lần — hook chạy mỗi touch event)
        static jmethodID mAction = nullptr, mX = nullptr;
        static jmethodID mY = nullptr, mPtrs = nullptr;
        static bool s_lookedUp = false;
        if (!s_lookedUp) {
            mAction = env->GetMethodID(motionClass, "getActionMasked", "()I");
            mX      = env->GetMethodID(motionClass, "getX",            "()F");
            mY      = env->GetMethodID(motionClass, "getY",            "()F");
            mPtrs   = env->GetMethodID(motionClass, "getPointerCount", "()I");
            s_lookedUp = (mAction && mX && mY && mPtrs);
        }
        if (mAction && mX && mY && mPtrs) {
            int   action = env->CallIntMethod(inputEvent, mAction);
            float x = env->CallFloatMethod(inputEvent, mX);
            float y = env->CallFloatMethod(inputEvent, mY);
            int   ptrs = env->CallIntMethod(inputEvent, mPtrs);

            // Touch ở SCREEN px, ImGui ở SURFACE px → convert chuẩn theo hướng ngang (landscape).
            int rw = Global::realW.load();
            int rh = Global::realH.load();
            float ix = x, iy = y;
            if (rw > 0 && rh > 0 && Global::screenWidth > 0 &&
                Global::screenHeight > 0) {
                int real_w = (rw > rh) ? rw : rh;
                int real_h = (rw > rh) ? rh : rw;
                ix = x * (float)Global::screenWidth  / (float)real_w;
                iy = y * (float)Global::screenHeight / (float)real_h;
            }

            // Capture: DOWN trong vùng menu → ngón này thuộc về menu tới UP.
            // Tap ngoài menu → game xử lý, KHÔNG đưa vào ImGui (tránh hero
            // di chuyển/đánh khi đang bấm menu, và ngược lại).
            bool isDown = (action == 0 || action == 5);          // DOWN
            bool isUp   = (action == 1 || action == 3 || action == 6); // UP/CANCEL
            bool captured = Global::touchCaptured.load();
            if (isDown &&
                (Global::wmRect.contains(ix, iy) ||
                 Global::menuRect.contains(ix, iy))) {
                captured = true;
                Global::touchCaptured.store(true);
            }
            if (captured) {
                ImGui_ImplAndroid_HandleInputEvent(action, ix, iy, ptrs);
                if (isUp) Global::touchCaptured.store(false);
                return JNI_TRUE; // nuốt — game không thấy tap này
            }
        }
    }
    return callOrig();
}

// ── JNI function-table patch: intercept RegisterNatives ─────────────────────
// VÌ SAO KHÔNG DÙNG api->hookJniNativeMethods:
//   Gọi nó trong preAppSpecialize làm ART abort fork game
//   ("JNI FatalError: Error calling post fork hooks", tombstone SIGABRT).
//   Nguyên nhân: fork usap64 chưa có app classpath → resolve class throw.
// Patch env->functions là pure memory write — không JNI call, không exception,
// an toàn bất cứ lúc nào. Khi UnityPlayer register natives (lúc game start),
// ta swap fnPtr của nativeInjectEvent — đồng thời log toàn bộ native methods
// để xác nhận signature thật.
using FnRegisterNatives = jint (*)(JNIEnv*, jclass, const JNINativeMethod*, jint);
static FnRegisterNatives orig_RegisterNatives = nullptr;

static std::string jniClassName(JNIEnv* env, jclass clazz) {
    std::string out;
    if (!env || !clazz) return out;
    jclass clsCls = env->GetObjectClass(clazz);
    if (!clsCls || env->ExceptionCheck()) { env->ExceptionClear(); return out; }
    jmethodID getName = env->GetMethodID(clsCls, "getName",
                                         "()Ljava/lang/String;");
    if (!getName || env->ExceptionCheck()) { env->ExceptionClear(); return out; }
    auto s = static_cast<jstring>(env->CallObjectMethod(clazz, getName));
    if (env->ExceptionCheck() || !s) { env->ExceptionClear(); return out; }
    const char* c = env->GetStringUTFChars(s, nullptr);
    if (c) {
        out = c;
        env->ReleaseStringUTFChars(s, c);
    }
    env->DeleteLocalRef(s);
    return out;
}

static jint hook_RegisterNatives(JNIEnv* env, jclass clazz,
                                 const JNINativeMethod* methods, jint n) {
    const JNINativeMethod* useMethods = methods;
    JNINativeMethod* copy = nullptr;

    std::string cls = jniClassName(env, clazz);
    bool unity = (cls.find("UnityPlayer") != std::string::npos);
    if (unity) {
        LOGI("[esp] RegisterNatives: %s (%d methods)", cls.c_str(), (int)n);
    }

    if (unity && methods && n > 0 && n < 512) {
        copy = static_cast<JNINativeMethod*>(
            malloc(sizeof(JNINativeMethod) * (size_t)n));
        if (copy) {
            memcpy(copy, methods, sizeof(JNINativeMethod) * (size_t)n);
            for (int i = 0; i < n; i++) {
                const char* nm = copy[i].name ? copy[i].name : "?";
                const char* sg = copy[i].signature ? copy[i].signature : "?";
                LOGI("[esp]   native: %s %s", nm, sg);
                if (strcmp(nm, "nativeInjectEvent") == 0 && copy[i].fnPtr) {
                    orig_nativeInjectEvent =
                        reinterpret_cast<FnNativeInjectEvent>(copy[i].fnPtr);
                    copy[i].fnPtr =
                        reinterpret_cast<void*>(hook_nativeInjectEvent);
                    LOGI("[esp] nativeInjectEvent HOOKED (%s)", sg);
                }
            }
            useMethods = copy;
        }
    }

    jint res = JNI_OK;
    if (orig_RegisterNatives) {
        res = orig_RegisterNatives(env, clazz, useMethods, n);
    }
    free(copy);
    return res;
}

// ── Touch Hook (routes Android touch to ImGui) ────────────────────────────────
static int hook_injectEvent(JNIEnv* env, jobject thiz, jobject inputEvent) {
    auto callOrig = [&]() -> int {
        if (orig_injectEvent) return orig_injectEvent(env, thiz, inputEvent);
        return 0;
    };
    // ImGui chưa init (frame đầu chưa vẽ) → không chạm vào ImGui state
    if (!s_imguiReady || !inputEvent) return callOrig();

    jclass motionClass = env->FindClass("android/view/MotionEvent");
    if (!motionClass || !env->IsInstanceOf(inputEvent, motionClass))
        return callOrig();

    jmethodID getAction = env->GetMethodID(motionClass, "getActionMasked", "()I");
    jmethodID getX      = env->GetMethodID(motionClass, "getX",            "()F");
    jmethodID getY      = env->GetMethodID(motionClass, "getY",            "()F");
    jmethodID getPtrs   = env->GetMethodID(motionClass, "getPointerCount", "()I");
    if (!getAction || !getX || !getY || !getPtrs) return callOrig();

    ImGui_ImplAndroid_HandleInputEvent(
        env->CallIntMethod(inputEvent, getAction),
        env->CallFloatMethod(inputEvent, getX),
        env->CallFloatMethod(inputEvent, getY),
        env->CallIntMethod(inputEvent, getPtrs));

    if (ImGui::GetIO().WantCaptureMouse) return 1; // nuốt event khi chạm vào menu
    return callOrig();
}

// ── ImGui Init ────────────────────────────────────────────────────────────────
static void applyUiScale(int w, int h) {
    if (w < 1 || h < 1) return;
    int shortSide = (w < h) ? w : h;
    float ns = (float)shortSide / 720.0f;
    if (ns < 0.85f) ns = 0.85f;
    if (ns > 2.20f) ns = 2.20f;
    float old = Global::uiScale <= 0.f ? 1.f : Global::uiScale;
    if (fabsf(ns - old) < 0.03f) return;
    Global::uiScale = ns;
    ImGui::GetIO().FontGlobalScale = ns;
    // Khôi phục style gốc và áp dụng tỉ lệ chuẩn duy nhất (tránh nhân dồn làm phình to menu)
    Menu::applyStyle();
    ImGui::GetStyle().ScaleAllSizes(ns);
    LOGI("[esp] UI scale: %.2f (surface %dx%d)", ns, w, h);
}

static void initImGui(int w, int h) {
    if (s_imguiReady || w < 1 || h < 1) return;

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize                      = ImVec2((float)w, (float)h);
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.IniFilename                      = nullptr;

    // Nạp Font hệ thống Android hỗ trợ Tiếng Việt có dấu (Unicode UTF-8)
    const ImWchar* glyph_ranges = io.Fonts->GetGlyphRangesVietnamese();
    float baseFontSize = 24.0f;
    const char* fontCandidates[] = {
        "/system/fonts/Roboto-Regular.ttf",
        "/system/fonts/NotoSansCJK-Regular.ttc",
        "/system/fonts/DroidSans.ttf",
        "/system/fonts/DroidSansFallback.ttf"
    };
    ImFont* menuRegular = nullptr;
    for (const char* path : fontCandidates) {
        if (access(path, R_OK) == 0) {
            menuRegular = io.Fonts->AddFontFromFileTTF(path, baseFontSize, nullptr, glyph_ranges);
            if (menuRegular) {
                LOGI("[esp] Da nap font he thong: %s", path);
                break;
            }
        }
    }
    if (!menuRegular) {
        menuRegular = io.Fonts->AddFontDefault();
        LOGW("[esp] Khong tim thay font he thong, dung default font");
    }
    ImFont* menuBold = nullptr;
    const char* boldCandidates[] = {
        "/system/fonts/Roboto-Bold.ttf", "/system/fonts/Roboto-Medium.ttf",
        "/system/fonts/NotoSans-Bold.ttf", "/system/fonts/DroidSans-Bold.ttf"
    };
    for (const char* path : boldCandidates) {
        if (access(path, R_OK) == 0) {
            menuBold = io.Fonts->AddFontFromFileTTF(path, baseFontSize, nullptr, glyph_ranges);
            if (menuBold) break;
        }
    }
    Menu::setFonts(menuRegular, menuBold);

    ImGui_ImplAndroid_Init();
    ImGui_ImplOpenGL3_Init("#version 300 es");

    // Scale lần đầu theo surface hiện tại (surface có thể đổi sau đó —
    // hook tự gọi applyUiScale khi phát hiện đổi kích thước).
    Menu::applyStyle();
    Global::uiScale = 1.0f; // gốc để applyUiScale tính tỉ số tương đối
    applyUiScale(w, h);
    s_imguiReady = true;
    LOGI("[esp] ImGui ready: %dx%d scale=%.2f", w, h, Global::uiScale);
}

// ── WorldToScreen (isometric MOBA camera) ─────────────────────────────────────
// AOV sử dụng camera isometric orthographic — dùng VP matrix chuẩn
static bool worldToScreen(const Vector3& world, float& outX, float& outY) {
    std::lock_guard<std::mutex> lk(Global::cameraMutex);
    const auto& vp = Global::viewProjMatrix;

    float cx = vp.m[0][0]*world.x + vp.m[0][1]*world.y + vp.m[0][2]*world.z + vp.m[0][3];
    float cy = vp.m[1][0]*world.x + vp.m[1][1]*world.y + vp.m[1][2]*world.z + vp.m[1][3];
    float cw = vp.m[3][0]*world.x + vp.m[3][1]*world.y + vp.m[3][2]*world.z + vp.m[3][3];

    if (cw < 0.001f) return false;

    outX = (cx / cw + 1.0f) * 0.5f * (float)Global::screenWidth;
    outY = (1.0f - cy / cw) * 0.5f * (float)Global::screenHeight;
    return true;
}

// ── MOBA Hero ESP Overlay ─────────────────────────────────────────────────────
static void renderHeroEspLegacy() {
    if (!Global::heroEspSettings.enabled.load()) return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;

    const float scale = Global::heroEspSettings.espScale;
    const float barW  = 70.f * scale;   // Chiều rộng thanh HP/Mana
    const float barH  = 6.f  * scale;   // Chiều cao thanh
    const float gap   = 3.f  * scale;   // Khoảng cách giữa các thanh

    // ESP = SỐ HỒI CHIÊU (thay chỗ tên tướng địch) + vòng tròn địch.
    // Chỉ vẽ tướng địch onScreen có dữ liệu cooldown.
    // Dữ liệu THẬT từ Game::refresh (dump 1.63.1.10).
    const auto& heroes = Game::heroes();
    for (const auto& h : heroes) {
        if (!h.onScreen || !h.isEnemy) continue;

        float sx = h.sx;
        float sy = h.sy;

        float topY = sy - 60.f * scale; // phía trên đầu tướng

        // ── Vòng đỏ dưới chân tướng địch ─────────────────────────────────────
        dl->AddCircle(ImVec2(sx, sy), 24.f * scale,
                      IM_COL32(255, 60, 60, 200), 32, 2.0f * scale);

        // ── 4 số hồi chiêu Q/W/E/R ──────────────────────────────────────────
        // Sẵn sàng → ô xanh + "·"; đang hồi → ô đỏ + số giây còn lại.
        // cd=-1 (chưa đọc được) → ô xám + "?".
        if (Global::heroEspSettings.showSkillCooldown.load()) {
            float cellW = (barW - 3.f * gap) / 4.f;
            float barX  = sx - barW / 2;

            for (int i = 0; i < 4; i++) {
                float cx = barX + i * (cellW + gap);
                int cd = h.cd[i];
                bool known = (cd >= 0);
                bool ready = known && (cd <= 0);
                ImU32 bgColor = !known ? IM_COL32(60, 60, 60, 200) :
                                ready  ? IM_COL32(30, 120, 30, 200)
                                       : IM_COL32(120, 30, 30, 220);
                dl->AddRectFilled(ImVec2(cx, topY),
                                  ImVec2(cx + cellW, topY + 16.f * scale),
                                  bgColor, 2.f);
                char cellStr[12];
                if (!known) snprintf(cellStr, sizeof(cellStr), "?");
                else if (ready) snprintf(cellStr, sizeof(cellStr), ".");
                else snprintf(cellStr, sizeof(cellStr), "%d", cd > 99 ? 99 : cd);
                dl->AddText(nullptr, 11.f * scale,
                            ImVec2(cx + cellW * 0.5f - 4.f * scale, topY + 2.f),
                            IM_COL32(240, 240, 240, 230), cellStr);
            }
            topY += 16.f * scale + gap;
        }
        (void)topY;
    }
}

// Compact cooldown row placed at the projected head position. This replaces
// the game's name-area visually without drawing circles or off-screen markers.
static void renderCooldownOverlay() {
    if (!Global::heroEspSettings.enabled.load() ||
        !Global::heroEspSettings.showSkillCooldown.load()) {
        return;
    }

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    ImFont* font = ImGui::GetFont();
    if (!draw || !font) return;

    float userScale = Global::heroEspSettings.espScale;
    if (userScale < 0.65f) userScale = 0.65f;
    if (userScale > 1.60f) userScale = 1.60f;
    const float scale = (Global::uiScale > 0.f ? Global::uiScale : 1.f) *
                        userScale;
    const float fontSize = 12.5f * scale;
    const float cellW = 36.f * scale;
    const float cellH = 18.f * scale;
    const float gap = 2.f * scale;
    const float panelW = cellW * 4.f + gap * 3.f;
    const char* labels[4] = {"Q", "W", "E", "R"};

    for (const Game::HeroInfo& hero : Game::heroes()) {
        if (!hero.isEnemy || !hero.onScreen) continue;

        int known = 0;
        for (int i = 0; i < 4; ++i) known += hero.cd[i] >= 0 ? 1 : 0;
        if (known == 0) continue;

        float left = hero.sx - panelW * 0.5f;
        float top = hero.sy - cellH - 7.f * scale;
        if (left < 0.f || left + panelW > Global::screenWidth || top < 0.f)
            continue;

        draw->AddRectFilled(
            ImVec2(left - 3.f * scale, top - 2.f * scale),
            ImVec2(left + panelW + 3.f * scale, top + cellH + 2.f * scale),
            IM_COL32(12, 16, 28, 210), 4.f * scale);
        draw->AddRect(
            ImVec2(left - 3.f * scale, top - 2.f * scale),
            ImVec2(left + panelW + 3.f * scale, top + cellH + 2.f * scale),
            IM_COL32(103, 232, 249, 155), 4.f * scale, 0,
            1.f * scale);

        for (int i = 0; i < 4; ++i) {
            char text[16];
            ImU32 color;
            if (hero.cd[i] < 0) {
                snprintf(text, sizeof(text), "%s:--", labels[i]);
                color = IM_COL32(126, 137, 158, 220);
            } else if (hero.cd[i] == 0) {
                snprintf(text, sizeof(text), "%s:0", labels[i]);
                color = IM_COL32(74, 222, 128, 255);
            } else {
                snprintf(text, sizeof(text), "%s:%d", labels[i],
                         hero.cd[i] > 999 ? 999 : hero.cd[i]);
                color = IM_COL32(251, 191, 36, 255);
            }

            float cellX = left + i * (cellW + gap);
            ImVec2 textSize = font->CalcTextSizeA(
                fontSize, cellW, 0.f, text, nullptr, nullptr);
            draw->AddText(
                font, fontSize,
                ImVec2(cellX + (cellW - textSize.x) * 0.5f,
                       top + (cellH - textSize.y) * 0.5f),
                color, text);
        }
    }
}


// ── Tower Range: ĐÃ XÓA theo yêu cầu (không phải tính năng cần cho LQM)

// ── eglSwapBuffers Hook ───────────────────────────────────────────────────────
static EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surf) {
    EGLint w = 0, h = 0;
    if (fn_eglQuerySurface) {
        fn_eglQuerySurface(dpy, surf, EGL_WIDTH,  &w);
        fn_eglQuerySurface(dpy, surf, EGL_HEIGHT, &h);
    }

    if (w > 0 && h > 0) {
        static int s_prevW = 0, s_prevH = 0;
        if (w != s_prevW || h != s_prevH) {
            LOGI("[esp] Kich thuoc man hinh doi: %dx%d -> %dx%d", s_prevW, s_prevH, w, h);
            s_prevW = w;
            s_prevH = h;
        }
        Global::screenWidth   = w;
        Global::screenHeight  = h;
        Global::screenCenterX = w * 0.5f;
        Global::screenCenterY = h * 0.5f;
    }

    if (!s_imguiReady) initImGui(w, h);

    if (s_imguiReady && w > 0 && h > 0) {
        ImGui::GetIO().DisplaySize = ImVec2((float)w, (float)h);
        applyUiScale(w, h);

        // Lưu Viewport hiện tại của game và ép Viewport khớp với toàn bộ Surface cho ImGui
        GLint last_viewport[4] = {0, 0, 0, 0};
        glGetIntegerv(GL_VIEWPORT, last_viewport);
        glViewport(0, 0, (GLsizei)w, (GLsizei)h);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplAndroid_NewFrame(w, h);

        Menu::render();
        Game::tick();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Khôi phục Viewport gốc cho game
        glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);

        static int s_frame = 0;
        if ((++s_frame % 30) == 0) Features::tick();
    }

    return orig_eglSwapBuffers(dpy, surf);
}

// ── Init ──────────────────────────────────────────────────────────────────────
namespace Esp {

bool patchJniTable(JNIEnv* env) {
    if (!env || !env->functions || !env->functions->RegisterNatives)
        return false;
    auto* table = const_cast<JNINativeInterface*>(env->functions);
    auto ours = static_cast<FnRegisterNatives>(hook_RegisterNatives);
    if (table->RegisterNatives == ours) return true; // đã là của mình
    orig_RegisterNatives = table->RegisterNatives;

    // Bảng JNI của ART 16 nằm ở vùng chỉ-đọc (RELRO) → mprotect RW trước.
    // Crash trước đây (SEGV_ACCERR) chính là ghi vào đây khi chưa mở quyền.
    long ps = sysconf(_SC_PAGESIZE);
    if (ps <= 0) ps = 4096;
    uintptr_t addr = reinterpret_cast<uintptr_t>(&table->RegisterNatives);
    uintptr_t page = addr & ~((uintptr_t)ps - 1);
    uintptr_t end  = (addr + sizeof(void*) + (uintptr_t)ps - 1) &
                     ~((uintptr_t)ps - 1);
    if (mprotect(reinterpret_cast<void*>(page), end - page,
                 PROT_READ | PROT_WRITE) != 0) {
        LOGE("[esp] mprotect RW fail: %s", strerror(errno));
        return false;
    }
    table->RegisterNatives = hook_RegisterNatives;
    mprotect(reinterpret_cast<void*>(page), end - page, PROT_READ);
    bool ok = (table->RegisterNatives ==
               static_cast<FnRegisterNatives>(hook_RegisterNatives));
    LOGI("[esp] JNI RegisterNatives %s (table=%p)",
         ok ? "intercepted" : "FAILED verify", (void*)table);
    return ok;
}

// Đảm bảo hook còn trong bảng JNI — ZygiskNext có thể patch đè sau mình,
// lúc đó UnityPlayer register sẽ đi đường khác, hook touch mất tác dụng.
// Chạy đầu hackThread (sớm, trước khi Unity load class) + lặp 15s.
void ensureTouchHook() {
    JNIEnv* zenv = Global::preEnv;
    if (!zenv) {
        LOGW("[esp] ensureTouchHook: no preEnv");
        return;
    }
    JavaVM* vm = nullptr;
    if (zenv->GetJavaVM(&vm) != 0 || !vm) {
        LOGW("[esp] ensureTouchHook: GetJavaVM fail");
        return;
    }
    JNIEnv* env = nullptr;
    if (vm->AttachCurrentThread(&env, nullptr) != 0 || !env) {
        LOGW("[esp] ensureTouchHook: attach fail");
        return;
    }
    auto* table = const_cast<JNINativeInterface*>(env->functions);
    auto ours = static_cast<FnRegisterNatives>(hook_RegisterNatives);
    for (int i = 0; i < 15; i++) {
        auto cur = table ? table->RegisterNatives : nullptr;
        LOGI("[esp] JNI table check %d: cur=%p ours=%p orig=%p", i,
             (void*)cur, (void*)ours, (void*)orig_RegisterNatives);
        if (cur == ours) break;
        patchJniTable(env);
        sleep(1);
    }
    vm->DetachCurrentThread();
}

// Lấy kích thước màn hình THẬT qua WindowManager (screen pixels).
// Chỉ dùng framework classes (android.*) nên FindClass an toàn từ mọi thread.
// Gọi 1 lần đầu hackThread. Thất bại → ratios giữ 1.0 (chế độ cũ).
void fetchRealDisplaySize() {
    JNIEnv* zenv = Global::preEnv;
    if (!zenv) return;
    JavaVM* vm = nullptr;
    if (zenv->GetJavaVM(&vm) != 0 || !vm) return;
    JNIEnv* env = nullptr;
    if (vm->AttachCurrentThread(&env, nullptr) != 0 || !env) return;

    int w = 0, h = 0;
    do {
        jclass atCls = env->FindClass("android/app/ActivityThread");
        if (!atCls || env->ExceptionCheck()) break;
        jmethodID curAT = env->GetStaticMethodID(
            atCls, "currentActivityThread", "()Landroid/app/ActivityThread;");
        if (!curAT || env->ExceptionCheck()) break;
        jobject at = env->CallStaticObjectMethod(atCls, curAT);
        if (!at || env->ExceptionCheck()) break;
        jmethodID getApp = env->GetMethodID(
            atCls, "getApplication", "()Landroid/app/Application;");
        if (!getApp || env->ExceptionCheck()) break;
        jobject app = env->CallObjectMethod(at, getApp);
        if (!app || env->ExceptionCheck()) break;

        jclass ctxCls = env->FindClass("android/content/Context");
        jclass appCls = env->GetObjectClass(app);
        jmethodID getSvc = env->GetMethodID(
            appCls, "getSystemService",
            "(Ljava/lang/String;)Ljava/lang/Object;");
        if (!getSvc || env->ExceptionCheck()) break;
        jfieldID fWin = env->GetStaticFieldID(
            ctxCls, "WINDOW_SERVICE", "Ljava/lang/String;");
        if (!fWin || env->ExceptionCheck()) break;
        auto svcName = static_cast<jstring>(
            env->GetStaticObjectField(ctxCls, fWin));
        jobject wm = env->CallObjectMethod(app, getSvc, svcName);
        if (!wm || env->ExceptionCheck()) break;

        jclass wmCls = env->GetObjectClass(wm);
        jmethodID getDisp = env->GetMethodID(
            wmCls, "getDefaultDisplay", "()Landroid/view/Display;");
        if (!getDisp || env->ExceptionCheck()) break;
        jobject disp = env->CallObjectMethod(wm, getDisp);
        if (!disp || env->ExceptionCheck()) break;

        jclass dispCls = env->GetObjectClass(disp);
        jclass dmCls = env->FindClass("android/util/DisplayMetrics");
        jmethodID dmCtor = env->GetMethodID(dmCls, "<init>", "()V");
        jmethodID getReal = env->GetMethodID(
            dispCls, "getRealMetrics", "(Landroid/util/DisplayMetrics;)V");
        if (!dmCtor || !getReal || env->ExceptionCheck()) break;
        jobject dm = env->NewObject(dmCls, dmCtor);
        env->CallVoidMethod(disp, getReal, dm);
        if (env->ExceptionCheck()) break;
        jfieldID fW = env->GetFieldID(dmCls, "widthPixels", "I");
        jfieldID fH = env->GetFieldID(dmCls, "heightPixels", "I");
        if (!fW || !fH || env->ExceptionCheck()) break;
        w = env->GetIntField(dm, fW);
        h = env->GetIntField(dm, fH);
        if (env->ExceptionCheck()) { w = h = 0; break; }
    } while (0);
    if (env->ExceptionCheck()) env->ExceptionClear();

    if (w > 0 && h > 0) {
        Global::realW.store(w);
        Global::realH.store(h);
        LOGI("[esp] real display: %dx%d (screen px)", w, h);
    } else {
        LOGW("[esp] real display FAILED — touch mapping = 1:1");
    }
    vm->DetachCurrentThread();
}

bool isTouchReady() {
    return orig_nativeInjectEvent != nullptr;
}

bool init() {
    // Hook eglSwapBuffers
    void* libEGL = dlopen("libEGL.so", RTLD_NOW | RTLD_GLOBAL);
    if (!libEGL) { LOGE("[esp] libEGL open fail"); return false; }

    void* swapFn = dlsym(libEGL, "eglSwapBuffers");
    fn_eglQuerySurface = reinterpret_cast<FnEglQuerySurface>(
        dlsym(libEGL, "eglQuerySurface"));

    if (!swapFn) { LOGE("[esp] eglSwapBuffers not found"); return false; }

    bool ok = Hook::hookAt(swapFn,
        reinterpret_cast<void*>(hook_eglSwapBuffers),
        reinterpret_cast<void**>(&orig_eglSwapBuffers));
    LOGI("[esp] eglSwapBuffers hook: %s", ok ? "OK" : "FAIL");

    // Hook inject_event cho touch input → ImGui
    uintptr_t injectAddr = Hook::findPattern(
        "libunity.so", Offsets::InjectEvent_Pattern);

    if (injectAddr) {
        bool touchOk = Hook::hookAt(
            reinterpret_cast<void*>(injectAddr),
            reinterpret_cast<void*>(hook_injectEvent),
            reinterpret_cast<void**>(&orig_injectEvent));
        LOGI("[esp] inject_event hook: %s at 0x%lx",
             touchOk ? "OK" : "FAIL", (unsigned long)injectAddr);
    } else {
        LOGW("[esp] inject_event pattern khong tim thay trong libunity.so");
        if (orig_nativeInjectEvent) {
            LOGI("[esp] JNI nativeInjectEvent is active; legacy fallback not needed");
        } else {
            LOGW("[esp] touch is waiting for JNI nativeInjectEvent registration");
        }
    }

    return ok;
}

} // namespace Esp
