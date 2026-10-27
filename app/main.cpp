// modular_synth: desktop app shell. Window + UI on the main thread; audio runs in
// the miniaudio callback thread and MIDI in RtMidi's thread. They only meet
// through SharedState (atomics and SPSC rings).

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "engine/Instrument.h"
#include "engine/ResourceManager.h"
#include "host/AudioHost.h"
#include "host/MidiHost.h"
#include "ui/App.h"
#include "ui/Theme.h"
#include "ui/UiState.h"

// --screenshot: write the framebuffer as a 24-bit BMP (used for automated UI checks)
static bool writeBmp(const char* path, int w, int h)
{
    std::vector<unsigned char> px((size_t)w * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_BGR_EXT, GL_UNSIGNED_BYTE, px.data());
    int row = (w * 3 + 3) & ~3;
    unsigned size = 54 + row * h;
    unsigned char hdr[54] = { 'B', 'M' };
    auto put32 = [&](int off, unsigned v) { for (int i = 0; i < 4; ++i) hdr[off + i] = (v >> (8 * i)) & 0xFF; };
    put32(2, size); put32(10, 54); put32(14, 40); put32(18, w); put32(22, h);
    hdr[26] = 1; hdr[28] = 24;
    FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    std::fwrite(hdr, 1, 54, f);
    std::vector<unsigned char> pad(row - w * 3, 0);
    for (int y = 0; y < h; ++y) {
        std::fwrite(&px[(size_t)y * w * 3], 1, w * 3, f);
        if (!pad.empty()) std::fwrite(pad.data(), 1, pad.size(), f);
    }
    std::fclose(f);
    return true;
}

int main(int argc, char** argv)
{
    // dev options: --screenshot out.bmp [--page N] [--preset N] [--demo] [--wtedit]
    //              [--set key=value]... [--fx N] [--lfotab N] [--filtertab N]
    // user option:  --asio [buffer]   start on the ASIO driver (a Focusrite if present)
    const char* shotPath = nullptr;
    int page = 0, preset = -1, fxSel = -1, lfoTab = 0, filterTab = 0;
    bool demo = false, wtEditor = false, useAsio = false;
    unsigned asioFrames = 128;
    std::vector<std::pair<std::string, float>> sets;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--screenshot") && i + 1 < argc) shotPath = argv[++i];
        else if (!std::strcmp(argv[i], "--page") && i + 1 < argc) page = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--preset") && i + 1 < argc) preset = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--demo")) demo = true;
        else if (!std::strcmp(argv[i], "--wtedit")) wtEditor = true;
        else if (!std::strcmp(argv[i], "--fx") && i + 1 < argc) fxSel = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--lfotab") && i + 1 < argc) lfoTab = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--filtertab") && i + 1 < argc) filterTab = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--asio")) {
            useAsio = true;
            if (i + 1 < argc && std::atoi(argv[i + 1]) > 0) asioFrames = (unsigned)std::atoi(argv[++i]);
        }
        else if (!std::strcmp(argv[i], "--set") && i + 1 < argc) {
            std::string kv = argv[++i];
            size_t eq = kv.find('=');
            if (eq != std::string::npos) sets.push_back({ kv.substr(0, eq), (float)std::atof(kv.c_str() + eq + 1) });
        }
    }

    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(1480, 920, "modular_synth", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwSetWindowSizeLimits(window, 1400, 820, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwMakeContextCurrent(window);
    // No vsync: the loop sleeps in glfwWaitEventsTimeout until the next frame is
    // due or an input event arrives, so key presses are handled immediately
    // instead of waiting for the next screen refresh.
    glfwSwapInterval(0);
    int refresh = 60;
    if (const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor())) refresh = std::max(mode->refreshRate, 30);
    const double frameTime = 1.0 / refresh;

    // set before ImGui's backend installs its callbacks: it chains to these
    glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int, int action, int mods) {
        if (auto* u = (UiState*)glfwGetWindowUserPointer(w))
            app::onKey(*u, key, action, (mods & (GLFW_MOD_CONTROL | GLFW_MOD_ALT)) != 0);
    });
    glfwSetWindowFocusCallback(window, [](GLFWwindow* w, int focused) {
        if (auto* u = (UiState*)glfwGetWindowUserPointer(w); u && !focused) app::releaseKeys(*u);
    });

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;   // fixed layout; nothing to persist

    float xs = 1.0f, ys = 1.0f;
    glfwGetWindowContentScale(window, &xs, &ys);
    theme::loadFonts(xs);
    theme::apply();
    ImGui::GetStyle().ScaleAllSizes(1.0f);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // ---- engine and devices ----
    auto shared = std::make_unique<SharedState>();
    auto instrument = std::make_unique<Instrument>(*shared);
    auto wavetables = std::make_unique<ResourceManager>(*shared);
    auto midi = std::make_unique<MidiHost>(*shared);
    auto audio = std::make_unique<AudioHost>();

    UiState ui(*shared, *wavetables, *midi, *audio);
    glfwSetWindowUserPointer(window, &ui);
    app::init(ui);                       // loads the Init preset (wavetables, params)

    // prepares the instrument, then starts the device; ASIO falls back to WASAPI if it can't open
    if (!useAsio || !audio->startAsio(*instrument, audio->preferredAsioDevice(), asioFrames))
        audio->start(*instrument, 5);
    midi->open(midi->preferredPort());

    ui.page = page;
    ui.wtEditorOpen = wtEditor;
    if (preset >= 0) app::loadPreset(ui, preset);
    for (auto& [key, v] : sets) {
        int p = findParam(key.c_str());
        if (p >= 0) shared->params.setValue((P)p, v);
    }
    if (fxSel >= 0) ui.fxSelected = fxSel;
    ui.lfoTab = lfoTab;
    ui.filterTab = filterTab;
    int frameCount = 0;

    double nextFrame = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        if (now < nextFrame) { glfwWaitEventsTimeout(nextFrame - now); continue; }   // input callbacks run in here
        nextFrame = std::max(nextFrame + frameTime, now);
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED)) { glfwWaitEventsTimeout(0.05); continue; }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (demo && frameCount == 20)
            for (int n : { 48, 60, 64, 67, 71 }) shared->uiEvents.push({ EventType::NoteOn, (uint8_t)n, 100 });

        app::frame(ui, glfwGetWindowAttrib(window, GLFW_FOCUSED) != 0);

        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(11 / 255.0f, 13 / 255.0f, 19 / 255.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (shotPath && ++frameCount == 150) {
            writeBmp(shotPath, w, h);
            glfwSetWindowShouldClose(window, 1);
        }
        glfwSwapBuffers(window);
    }

    // stop devices before tearing down what their threads use
    midi->close();
    audio->stop();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
