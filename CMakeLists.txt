cmake_minimum_required(VERSION 3.22)
project(imgui_android_overlay LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Dear ImGui is a git submodule. Clone it before configuring:
#   git submodule update --init --recursive
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
if(NOT EXISTS "${IMGUI_DIR}/imgui.h")
    message(FATAL_ERROR
        "Dear ImGui not found at ${IMGUI_DIR}.\n"
        "Run: git submodule update --init --recursive")
endif()

# ---------------------------------------------------------------------------
# imgui — core + official OpenGL ES 3 renderer
# ---------------------------------------------------------------------------
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
target_compile_definitions(imgui PUBLIC
    IMGUI_IMPL_OPENGL_ES3
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS
)
if(ANDROID)
    target_link_libraries(imgui PUBLIC GLESv3)
endif()

# ---------------------------------------------------------------------------
# imgui_android_overlay — public API + custom Android platform backend
# ---------------------------------------------------------------------------
add_library(imgui_android_overlay STATIC
    src/overlay.cpp
    src/imgui_impl_android.cpp
)
target_include_directories(imgui_android_overlay
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE
        $<$<BOOL:${ANDROID}>:${ANDROID_NDK}/sources/android/native_app_glue>
)
target_link_libraries(imgui_android_overlay PUBLIC imgui)
if(ANDROID)
    target_link_libraries(imgui_android_overlay PUBLIC android EGL GLESv3 log)
endif()

# Optional native-activity sample. Off by default — this tree is a library.
option(IMGUI_OVERLAY_BUILD_SAMPLE "Build the android_native_app_glue sample" OFF)
if(ANDROID AND IMGUI_OVERLAY_BUILD_SAMPLE)
    add_library(android_native_app_glue STATIC
        ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
    )
    target_include_directories(android_native_app_glue PUBLIC
        ${ANDROID_NDK}/sources/android/native_app_glue
    )
    add_library(overlay_sample SHARED sample/main.cpp)
    target_link_libraries(overlay_sample PRIVATE
        imgui_android_overlay
        android_native_app_glue
        android
        EGL
        GLESv3
        log
    )
    target_link_options(overlay_sample PRIVATE -u ANativeActivity_onCreate)
endif()
