// Dear ImGui: Zygisk-friendly Android platform backend (touch via raw values)
// See imgui_impl_android.h for documentation.

#include "imgui.h"
#ifndef IMGUI_DISABLE
#include "imgui_impl_android.h"
#include <time.h>

// Masked AMotionEvent actions (same values as android/input.h)
enum {
    MOTION_ACTION_DOWN         = 0,
    MOTION_ACTION_UP           = 1,
    MOTION_ACTION_MOVE         = 2,
    MOTION_ACTION_CANCEL       = 3,
    MOTION_ACTION_POINTER_DOWN = 5,
    MOTION_ACTION_POINTER_UP   = 6,
};

// Backend data
static double g_Time = 0.0;

void ImGui_ImplAndroid_Init()
{
    IMGUI_CHECKVERSION();
    g_Time = 0.0;

    // Setup backend capabilities flags
    ImGuiIO& io = ImGui::GetIO();
    io.BackendPlatformName = "imgui_impl_android_zygisk";
}

void ImGui_ImplAndroid_Shutdown()
{
    ImGuiIO& io = ImGui::GetIO();
    io.BackendPlatformName = nullptr;
}

void ImGui_ImplAndroid_NewFrame(int fb_width, int fb_height)
{
    ImGuiIO& io = ImGui::GetIO();

    // Setup display size (from eglQuerySurface — every frame, cheap)
    if (fb_width > 0 && fb_height > 0)
        io.DisplaySize = ImVec2((float)fb_width, (float)fb_height);

    // Setup time step
    struct timespec current_timespec;
    clock_gettime(CLOCK_MONOTONIC, &current_timespec);
    double current_time = (double)(current_timespec.tv_sec) + (current_timespec.tv_nsec / 1000000000.0);
    io.DeltaTime = g_Time > 0.0 ? (float)(current_time - g_Time) : (float)(1.0f / 60.0f);
    g_Time = current_time;
}

int32_t ImGui_ImplAndroid_HandleInputEvent(int action, float x, float y, int pointer_count)
{
    (void)pointer_count;
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);

    switch (action)
    {
    case MOTION_ACTION_DOWN:
    case MOTION_ACTION_POINTER_DOWN:
        io.AddMousePosEvent(x, y);
        io.AddMouseButtonEvent(0, true);
        break;
    case MOTION_ACTION_MOVE:
        io.AddMousePosEvent(x, y);
        break;
    case MOTION_ACTION_UP:
    case MOTION_ACTION_POINTER_UP:
    case MOTION_ACTION_CANCEL:
        io.AddMousePosEvent(x, y);
        io.AddMouseButtonEvent(0, false);
        break;
    default:
        break;
    }

    return io.WantCaptureMouse ? 1 : 0;
}

//-----------------------------------------------------------------------------

#endif // #ifndef IMGUI_DISABLE
