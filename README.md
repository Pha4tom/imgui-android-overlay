# imgui-android-overlay

Drop-in Dear ImGui debug overlay for Android NDK apps and games. No root — it
runs entirely inside the host process, over your own OpenGL ES 3.0 surface.

Think browser-dev-tools, but for a native activity: live FPS, tweakable
variables, a log ring, and a command console.

## Integration

Four calls. That's the whole lifecycle.

```cpp
// APP_CMD_INIT_WINDOW — EGL context must already be current
DebugOverlay::Init(app);

// onInputEvent
return ImGui_ImplAndroid_HandleInputEvent(event);

// each frame, after your draw, before eglSwapBuffers
DebugOverlay::Render();

// APP_CMD_TERM_WINDOW / exit
DebugOverlay::Shutdown();
```

CMake:

```cmake
add_subdirectory(imgui-android-overlay)
target_link_libraries(your_game PRIVATE imgui_android_overlay)
```

Dear ImGui is a git submodule, not vendored:

```
git submodule update --init --recursive
```

The overlay target (`imgui_android_overlay`) links `imgui` plus Android system
libs: `android`, `EGL`, `GLESv3`, `log`.

## Visibility

The overlay starts **visible**. Hide/show with:

- a **three-finger tap** (no physical keys on mobile)
- `DebugOverlay::SetVisible(bool)` / `IsVisible()` from host code

## Tabs

### Stats

Live FPS from ImGui's `io.Framerate`, plus a rolling 120-frame line graph.

### Watch

Host code exposes raw pointers. The overlay edits them in place, every frame.

```cpp
DebugOverlay::Watch("gravity", &gravity, 0.0f, 20.0f); // slider
DebugOverlay::Watch("score",   &score,   0,    9999);  // slider
DebugOverlay::Watch("paused",  &paused);               // checkbox
```

Keyed by label. Re-registering the same name just updates the pointer/range.

### Log

```cpp
DebugOverlay::Log("spawned %d orbs at y=%.1f", n, y);
```

In-memory ring of 500 lines (oldest dropped). Scrollable view, auto-scrolls on
new lines, **Clear** button.

### Console

One text field + **Run**. Host registers named commands:

```cpp
DebugOverlay::RegisterCommand("reset", [](const char* args) {
    score = 0;
    DebugOverlay::Log("reset (%s)", args);
});
```

Typed input is split into `command_name` + `args` and dispatched. The line is
echoed to the log. Unknown names log `unknown command: …`.

## Platform backend

There is no overlay-specific code in upstream ImGui that does a three-finger
toggle, first-finger mouse emulation, and this window layout. This repo ships
its own `imgui_impl_android.cpp`:

| | |
|---|---|
| Renderer | OpenGL ES 3.0 via official `imgui_impl_opengl3` (`#version 300 es`) |
| Display | `ANativeWindow` width/height read every frame into `io.DisplaySize` |
| Pointer | first touch → ImGui mouse position / button 0 |
| Gesture | 3-finger `ACTION_POINTER_DOWN` → show/hide |
| Keys | small `AKEYCODE_*` → `ImGuiKey` map (hardware keys only) |

### Known gaps

- no real multi-touch gestures beyond the toggle
- no on-screen keyboard / Unicode text-input wiring
- no DPI / density scaling (`io.DisplayFramebufferScale` is 1,1)

## Sample

[`sample/main.cpp`](sample/main.cpp) is a native-activity host that:

- registers `gravity` (float), `score` (int), `paused` (bool)
- registers a `reset` command
- logs a startup line

It has no game. `TODO` comments mark where a real render pass and
`eglSwapBuffers` belong. Build it with `-DIMGUI_OVERLAY_BUILD_SAMPLE=ON` from
an Android NDK CMake toolchain.

## Layout

```
├── CMakeLists.txt
├── src/
│   ├── overlay.h / overlay.cpp          public API + window layout
│   └── imgui_impl_android.h / .cpp      custom platform backend
├── third_party/imgui/                   submodule (not committed)
├── sample/main.cpp
└── README.md
```

## License

MIT. Dear ImGui is MIT as well — you must clone the submodule separately.
