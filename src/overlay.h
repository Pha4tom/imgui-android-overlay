// imgui-android-overlay — public API
//
// Drop-in Dear ImGui debug overlay for Android NDK apps.
// Runs inside the host process. No root, no extra Activity.
//
// Lifecycle (from android_native_app_glue):
//   APP_CMD_INIT_WINDOW  -> DebugOverlay::Init(app)
//   onInputEvent         -> ImGui_ImplAndroid_HandleInputEvent(event)
//   each frame, after your draw, before eglSwapBuffers
//                        -> DebugOverlay::Render()
//   APP_CMD_TERM_WINDOW / exit
//                        -> DebugOverlay::Shutdown()

#pragma once

#include <cstdint>
#include <functional>

struct android_app;
struct AInputEvent;

namespace DebugOverlay {

// Create the ImGui context, bind the Android + OpenGL ES 3 backends.
// The EGL context must already be current. Overlay starts visible.
bool Init(android_app* app);

// NewFrame + draw the overlay window + upload ImGui draw data.
// Call once per frame after the host's own draw calls.
void Render();

void Shutdown();

void SetVisible(bool visible);
bool IsVisible();

// Expose a host variable as a live widget. Re-registering the same label
// updates the pointer and range in place (hash map keyed by label).
void Watch(const char* label, float* value, float min, float max);
void Watch(const char* label, int* value, int min, int max);
void Watch(const char* label, bool* value);

// printf-style log into a 500-line ring buffer (oldest dropped).
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 1, 2)))
#endif
void Log(const char* fmt, ...);

// Named console command. `args` is everything after the first whitespace
// of the typed line (may be empty).
using CommandFn = std::function<void(const char* args)>;
void RegisterCommand(const char* name, CommandFn callback);

} // namespace DebugOverlay
