// Dear ImGui: Zygisk-friendly Android platform backend (touch via raw values)
//
// DIFFERENCES vs stock imgui_impl_android:
//  - Stock backend needs ANativeWindow* + AInputEvent* (we have neither in a
//    Zygisk-injected game process).
//  - This backend instead takes:
//      * screen size (from eglQuerySurface) in NewFrame()
//      * raw touch values (action, x, y) forwarded from the libunity.so
//        inject_event hook in HandleInputEvent()
//
// API matches what jni/src/esp/esp.cpp expects:
//    ImGui_ImplAndroid_Init();
//    ImGui_ImplAndroid_HandleInputEvent(action, x, y, pointerCount);
//    ImGui_ImplAndroid_NewFrame(fbWidth, fbHeight);
//
// Touch action values are Android AMotionEvent masked actions:
//    0 = DOWN, 1 = UP, 2 = MOVE, 3 = CANCEL, 5 = POINTER_DOWN, 6 = POINTER_UP

#pragma once
#include "imgui.h"      // IMGUI_IMPL_API
#include <stdint.h>
#ifndef IMGUI_DISABLE

// Follow "Getting Started" link and check examples/ folder to learn about using backends!
IMGUI_IMPL_API void    ImGui_ImplAndroid_Init();
IMGUI_IMPL_API void    ImGui_ImplAndroid_Shutdown();
IMGUI_IMPL_API void    ImGui_ImplAndroid_NewFrame(int fb_width, int fb_height);
IMGUI_IMPL_API int32_t ImGui_ImplAndroid_HandleInputEvent(int action, float x, float y, int pointer_count);

#endif // #ifndef IMGUI_DISABLE
