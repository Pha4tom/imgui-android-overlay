#include "imgui_impl_android.h"

#ifndef IMGUI_DISABLE

#include <cfloat>
#include <ctime>
#include <android/input.h>
#include <android/keycodes.h>
#include <android/native_window.h>

static double         g_Time            = 0.0;
static ANativeWindow* g_Window          = nullptr;
static void         (*g_ToggleCallback)() = nullptr;

void ImGui_ImplAndroid_SetToggleCallback(void (*callback)())
{
    g_ToggleCallback = callback;
}

static ImGuiKey KeyCodeToImGuiKey(int32_t key_code)
{
    switch (key_code)
    {
    case AKEYCODE_TAB:           return ImGuiKey_Tab;
    case AKEYCODE_DPAD_LEFT:     return ImGuiKey_LeftArrow;
    case AKEYCODE_DPAD_RIGHT:    return ImGuiKey_RightArrow;
    case AKEYCODE_DPAD_UP:       return ImGuiKey_UpArrow;
    case AKEYCODE_DPAD_DOWN:     return ImGuiKey_DownArrow;
    case AKEYCODE_MOVE_HOME:     return ImGuiKey_Home;
    case AKEYCODE_MOVE_END:      return ImGuiKey_End;
    case AKEYCODE_FORWARD_DEL:   return ImGuiKey_Delete;
    case AKEYCODE_DEL:           return ImGuiKey_Backspace;
    case AKEYCODE_SPACE:         return ImGuiKey_Space;
    case AKEYCODE_ENTER:         return ImGuiKey_Enter;
    case AKEYCODE_ESCAPE:        return ImGuiKey_Escape;
    case AKEYCODE_GRAVE:         return ImGuiKey_GraveAccent;
    case AKEYCODE_PAGE_UP:       return ImGuiKey_PageUp;
    case AKEYCODE_PAGE_DOWN:     return ImGuiKey_PageDown;
    case AKEYCODE_A:             return ImGuiKey_A;
    case AKEYCODE_C:             return ImGuiKey_C;
    case AKEYCODE_V:             return ImGuiKey_V;
    case AKEYCODE_X:             return ImGuiKey_X;
    case AKEYCODE_Y:             return ImGuiKey_Y;
    case AKEYCODE_Z:             return ImGuiKey_Z;
    default:                     return ImGuiKey_None;
    }
}

bool ImGui_ImplAndroid_Init(ANativeWindow* window)
{
    g_Window = window;
    g_Time   = 0.0;

    ImGuiIO& io = ImGui::GetIO();
    io.BackendPlatformName = "imgui_impl_android (overlay)";
    return true;
}

void ImGui_ImplAndroid_Shutdown()
{
    ImGuiIO& io = ImGui::GetIO();
    io.BackendPlatformName = nullptr;
    g_Window          = nullptr;
    g_ToggleCallback  = nullptr;
    g_Time            = 0.0;
}

void ImGui_ImplAndroid_NewFrame()
{
    ImGuiIO& io = ImGui::GetIO();

    int32_t w = g_Window ? ANativeWindow_getWidth(g_Window)  : 0;
    int32_t h = g_Window ? ANativeWindow_getHeight(g_Window) : 0;
    io.DisplaySize = ImVec2((float)w, (float)h);
    // DPI scaling is a known gap — keep framebuffer scale at 1:1.
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double now = (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
    io.DeltaTime = g_Time > 0.0 ? (float)(now - g_Time) : (1.0f / 60.0f);
    g_Time = now;
}

int32_t ImGui_ImplAndroid_HandleInputEvent(const AInputEvent* input_event)
{
    if (!input_event)
        return 0;

    ImGuiIO& io = ImGui::GetIO();
    const int32_t event_type = AInputEvent_getType(input_event);

    if (event_type == AINPUT_EVENT_TYPE_KEY)
    {
        const int32_t key_code   = AKeyEvent_getKeyCode(input_event);
        const int32_t scan_code  = AKeyEvent_getScanCode(input_event);
        const int32_t action     = AKeyEvent_getAction(input_event);
        const int32_t meta       = AKeyEvent_getMetaState(input_event);

        io.AddKeyEvent(ImGuiMod_Ctrl,  (meta & AMETA_CTRL_ON)  != 0);
        io.AddKeyEvent(ImGuiMod_Shift, (meta & AMETA_SHIFT_ON) != 0);
        io.AddKeyEvent(ImGuiMod_Alt,   (meta & AMETA_ALT_ON)   != 0);
        io.AddKeyEvent(ImGuiMod_Super, (meta & AMETA_META_ON)  != 0);

        if (action == AKEY_EVENT_ACTION_DOWN || action == AKEY_EVENT_ACTION_UP)
        {
            const ImGuiKey key = KeyCodeToImGuiKey(key_code);
            if (key != ImGuiKey_None)
            {
                io.AddKeyEvent(key, action == AKEY_EVENT_ACTION_DOWN);
                io.SetKeyEventNativeData(key, key_code, scan_code);
            }
        }
        return 1;
    }

    if (event_type != AINPUT_EVENT_TYPE_MOTION)
        return 0;

    int32_t action = AMotionEvent_getAction(input_event);
    const int32_t pointer_index =
        (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
        AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    action &= AMOTION_EVENT_ACTION_MASK;

    const int32_t pointer_count = AMotionEvent_getPointerCount(input_event);

    // Three-finger tap: a new pointer going down that brings the count to 3.
    if (action == AMOTION_EVENT_ACTION_POINTER_DOWN && pointer_count == 3)
    {
        if (g_ToggleCallback)
            g_ToggleCallback();
        return 1;
    }

    io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);

    // Single-finger mouse emulation — always the first pointer.
    const float x = AMotionEvent_getX(input_event, 0);
    const float y = AMotionEvent_getY(input_event, 0);

    switch (action)
    {
    case AMOTION_EVENT_ACTION_DOWN:
        io.AddMousePosEvent(x, y);
        io.AddMouseButtonEvent(0, true);
        break;
    case AMOTION_EVENT_ACTION_UP:
        io.AddMousePosEvent(x, y);
        io.AddMouseButtonEvent(0, false);
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        break;
    case AMOTION_EVENT_ACTION_MOVE:
        if (pointer_count == 1 || pointer_index == 0)
            io.AddMousePosEvent(x, y);
        break;
    default:
        break;
    }

    return 1;
}

#endif // #ifndef IMGUI_DISABLE
