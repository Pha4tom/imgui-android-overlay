// Minimal android_native_app_glue host.
// Demonstrates wiring — not a real game. TODO markers show where your
// render pass and eglSwapBuffers belong.

#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/log.h>

#include "overlay.h"
#include "imgui_impl_android.h"

#define LOG_TAG "overlay_sample"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

struct Engine
{
    android_app* app     = nullptr;
    EGLDisplay   display = EGL_NO_DISPLAY;
    EGLSurface   surface = EGL_NO_SURFACE;
    EGLContext   context = EGL_NO_CONTEXT;
    bool         ready   = false;
};

static float g_gravity = 9.8f;
static int   g_score   = 0;
static bool  g_paused  = false;

static void engine_term_display(Engine* e)
{
    if (e->display != EGL_NO_DISPLAY)
    {
        eglMakeCurrent(e->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (e->context != EGL_NO_CONTEXT)
            eglDestroyContext(e->display, e->context);
        if (e->surface != EGL_NO_SURFACE)
            eglDestroySurface(e->display, e->surface);
        eglTerminate(e->display);
    }
    e->display = EGL_NO_DISPLAY;
    e->context = EGL_NO_CONTEXT;
    e->surface = EGL_NO_SURFACE;
    e->ready   = false;
}

static bool engine_init_display(Engine* e)
{
    if (!e->app || !e->app->window)
        return false;

    e->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(e->display, nullptr, nullptr);

    const EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8,
        EGL_NONE
    };
    EGLConfig config;
    EGLint    num = 0;
    eglChooseConfig(e->display, attribs, &config, 1, &num);
    if (num == 0)
        return false;

    EGLint format = 0;
    eglGetConfigAttrib(e->display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(e->app->window, 0, 0, format);

    e->surface = eglCreateWindowSurface(e->display, config, e->app->window, nullptr);
    const EGLint ctx[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    e->context = eglCreateContext(e->display, config, EGL_NO_CONTEXT, ctx);
    if (eglMakeCurrent(e->display, e->surface, e->surface, e->context) == EGL_FALSE)
        return false;

    e->ready = true;
    return true;
}

static void engine_draw(Engine* e)
{
    if (!e->ready)
        return;

    // TODO: your game's render pass goes here.
    glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    DebugOverlay::Render();

    // TODO: eglSwapBuffers — required so the overlay actually presents.
    eglSwapBuffers(e->display, e->surface);
}

static void on_app_cmd(android_app* app, int32_t cmd)
{
    auto* e = static_cast<Engine*>(app->userData);
    switch (cmd)
    {
    case APP_CMD_INIT_WINDOW:
        if (!engine_init_display(e))
        {
            LOGI("EGL init failed");
            break;
        }
        if (!DebugOverlay::Init(app))
        {
            LOGI("DebugOverlay::Init failed");
            break;
        }
        DebugOverlay::Watch("gravity", &g_gravity, 0.0f, 20.0f);
        DebugOverlay::Watch("score", &g_score, 0, 9999);
        DebugOverlay::Watch("paused", &g_paused);
        DebugOverlay::RegisterCommand("reset", [](const char* /*args*/) {
            g_score = 0;
            DebugOverlay::Log("reset: score cleared");
        });
        DebugOverlay::Log("sample host ready — 3-finger tap toggles overlay");
        break;
    case APP_CMD_TERM_WINDOW:
        DebugOverlay::Shutdown();
        engine_term_display(e);
        break;
    default:
        break;
    }
}

static int32_t on_input_event(android_app* /*app*/, AInputEvent* event)
{
    return ImGui_ImplAndroid_HandleInputEvent(event);
}

void android_main(android_app* app)
{
    Engine engine;
    engine.app        = app;
    app->userData     = &engine;
    app->onAppCmd     = on_app_cmd;
    app->onInputEvent = on_input_event;

    while (true)
    {
        int                  events = 0;
        android_poll_source* source = nullptr;
        while (ALooper_pollOnce(engine.ready ? 0 : -1, nullptr, &events,
                                reinterpret_cast<void**>(&source)) >= 0)
        {
            if (source)
                source->process(app, source);
            if (app->destroyRequested)
            {
                DebugOverlay::Shutdown();
                engine_term_display(&engine);
                return;
            }
        }

        if (engine.ready)
        {
            // TODO: your game update (respect g_paused / g_gravity).
            engine_draw(&engine);
        }
    }
}
