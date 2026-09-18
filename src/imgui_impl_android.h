// Custom Dear ImGui platform backend for Android native apps.
// Pair with imgui_impl_opengl3 (OpenGL ES 3.0).
//
// Implemented:
//   [X] Display size from ANativeWindow every frame
//   [X] First-finger touch mapped to ImGui mouse pos / button 0
//   [X] 3-finger ACTION_POINTER_DOWN toggles overlay visibility
//   [X] Basic hardware-key -> ImGuiKey mapping
//
// Known gaps (see README):
//   [ ] Multi-touch gestures beyond the 3-finger toggle
//   [ ] On-screen keyboard / Unicode text input wiring
//   [ ] DPI / density scaling (io.DisplayFramebufferScale stays 1,1)

#pragma once

#include "imgui.h"
#include <cstdint>

struct ANativeWindow;
struct AInputEvent;

IMGUI_IMPL_API bool    ImGui_ImplAndroid_Init(ANativeWindow* window);
IMGUI_IMPL_API void    ImGui_ImplAndroid_Shutdown();
IMGUI_IMPL_API void    ImGui_ImplAndroid_NewFrame();
IMGUI_IMPL_API int32_t ImGui_ImplAndroid_HandleInputEvent(const AInputEvent* input_event);

// Overlay registers this so a 3-finger tap can show/hide the window
// without the backend depending on DebugOverlay.
IMGUI_IMPL_API void    ImGui_ImplAndroid_SetToggleCallback(void (*callback)());
