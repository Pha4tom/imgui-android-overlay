#include "overlay.h"
#include "imgui_impl_android.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"

#include <android_native_app_glue.h>

#include <cstdarg>
#include <cstdio>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

namespace DebugOverlay {
namespace {

constexpr int kMaxLogLines     = 500;
constexpr int kFpsHistory      = 120;
constexpr int kConsoleBufSize  = 256;

bool g_initialized = false;
bool g_visible     = true;

struct FloatWatch { float* ptr; float min; float max; };
struct IntWatch   { int*   ptr; int   min; int   max; };
struct BoolWatch  { bool*  ptr; };

std::unordered_map<std::string, FloatWatch> g_floats;
std::unordered_map<std::string, IntWatch>   g_ints;
std::unordered_map<std::string, BoolWatch>  g_bools;
std::vector<std::string>                    g_float_order;
std::vector<std::string>                    g_int_order;
std::vector<std::string>                    g_bool_order;

std::deque<std::string>                     g_log;
bool                                        g_log_scroll = false;

std::unordered_map<std::string, CommandFn>  g_commands;
char                                        g_console[kConsoleBufSize] = {};

float g_fps_hist[kFpsHistory] = {};
int   g_fps_offset            = 0;

void ToggleVisible()
{
    g_visible = !g_visible;
}

template <typename Map, typename Vec>
void RememberOrder(Map& /*map*/, Vec& order, const std::string& label)
{
    for (const auto& existing : order)
        if (existing == label)
            return;
    order.push_back(label);
}

void DrawStats()
{
    ImGuiIO& io = ImGui::GetIO();
    g_fps_hist[g_fps_offset] = io.Framerate;
    g_fps_offset = (g_fps_offset + 1) % kFpsHistory;

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                1000.0f / (io.Framerate > 0.1f ? io.Framerate : 0.1f),
                io.Framerate);
    ImGui::PlotLines("##fps",
                     g_fps_hist,
                     kFpsHistory,
                     g_fps_offset,
                     nullptr,
                     0.0f,
                     120.0f,
                     ImVec2(-1.0f, 80.0f));
}

void DrawWatch()
{
    if (g_float_order.empty() && g_int_order.empty() && g_bool_order.empty())
    {
        ImGui::TextDisabled("No watches. Host calls Watch(\"name\", &var, min, max).");
        return;
    }
    for (const auto& label : g_float_order)
    {
        auto it = g_floats.find(label);
        if (it != g_floats.end() && it->second.ptr)
            ImGui::SliderFloat(label.c_str(), it->second.ptr, it->second.min, it->second.max);
    }
    for (const auto& label : g_int_order)
    {
        auto it = g_ints.find(label);
        if (it != g_ints.end() && it->second.ptr)
            ImGui::SliderInt(label.c_str(), it->second.ptr, it->second.min, it->second.max);
    }
    for (const auto& label : g_bool_order)
    {
        auto it = g_bools.find(label);
        if (it != g_bools.end() && it->second.ptr)
            ImGui::Checkbox(label.c_str(), it->second.ptr);
    }
}

void DrawLog()
{
    if (ImGui::Button("Clear"))
        g_log.clear();
    ImGui::Separator();
    ImGui::BeginChild("log_scroll", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
    for (const auto& line : g_log)
        ImGui::TextUnformatted(line.c_str());
    if (g_log_scroll)
    {
        ImGui::SetScrollHereY(1.0f);
        g_log_scroll = false;
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();
}

void RunConsoleLine(const char* line)
{
    if (!line || !line[0])
        return;

    Log("> %s", line);

    std::string text(line);
    const auto  sp   = text.find_first_of(" \t");
    std::string name = (sp == std::string::npos) ? text : text.substr(0, sp);
    std::string args = (sp == std::string::npos) ? std::string() : text.substr(sp + 1);

    auto it = g_commands.find(name);
    if (it == g_commands.end())
    {
        Log("unknown command: %s", name.c_str());
        return;
    }
    it->second(args.c_str());
}

void DrawConsole()
{
    const bool enter = ImGui::InputText("##cmd",
                                        g_console,
                                        kConsoleBufSize,
                                        ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    const bool run = ImGui::Button("Run");
    if (enter || run)
    {
        RunConsoleLine(g_console);
        g_console[0] = '\0';
        ImGui::SetKeyboardFocusHere(-1);
    }
}

} // namespace

bool Init(android_app* app)
{
    if (g_initialized)
        return true;
    if (!app || !app->window)
        return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // no writable path assumed
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.TouchExtraPadding = ImVec2(6.0f, 6.0f);
    style.WindowRounding    = 4.0f;
    style.FrameRounding     = 3.0f;
    style.GrabRounding      = 2.0f;
    style.ScaleAllSizes(1.25f);

    if (!ImGui_ImplAndroid_Init(app->window))
        return false;
    ImGui_ImplAndroid_SetToggleCallback(&ToggleVisible);

    // GL context must already be current.
    if (!ImGui_ImplOpenGL3_Init("#version 300 es"))
        return false;

    g_visible      = true;
    g_initialized  = true;
    Log("DebugOverlay::Init — GLES3 backend ready");
    return true;
}

void Render()
{
    if (!g_initialized)
        return;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();

    if (g_visible)
    {
        ImGui::SetNextWindowSize(ImVec2(360.0f, 420.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Debug Overlay", &g_visible))
        {
            if (ImGui::BeginTabBar("overlay_tabs"))
            {
                if (ImGui::BeginTabItem("Stats"))
                {
                    DrawStats();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Watch"))
                {
                    DrawWatch();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Log"))
                {
                    DrawLog();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Console"))
                {
                    DrawConsole();
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Shutdown()
{
    if (!g_initialized)
        return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplAndroid_Shutdown();
    ImGui::DestroyContext();

    g_floats.clear();
    g_ints.clear();
    g_bools.clear();
    g_float_order.clear();
    g_int_order.clear();
    g_bool_order.clear();
    g_log.clear();
    g_commands.clear();
    g_console[0]   = '\0';
    g_initialized  = false;
}

void SetVisible(bool visible) { g_visible = visible; }
bool IsVisible()              { return g_visible; }

void Watch(const char* label, float* value, float min, float max)
{
    if (!label || !value)
        return;
    std::string key(label);
    g_floats[key] = FloatWatch{value, min, max};
    RememberOrder(g_floats, g_float_order, key);
}

void Watch(const char* label, int* value, int min, int max)
{
    if (!label || !value)
        return;
    std::string key(label);
    g_ints[key] = IntWatch{value, min, max};
    RememberOrder(g_ints, g_int_order, key);
}

void Watch(const char* label, bool* value)
{
    if (!label || !value)
        return;
    std::string key(label);
    g_bools[key] = BoolWatch{value};
    RememberOrder(g_bools, g_bool_order, key);
}

void Log(const char* fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    g_log.emplace_back(buf);
    while ((int)g_log.size() > kMaxLogLines)
        g_log.pop_front();
    g_log_scroll = true;
}

void RegisterCommand(const char* name, CommandFn callback)
{
    if (!name || !callback)
        return;
    g_commands[std::string(name)] = std::move(callback);
}

} // namespace DebugOverlay
