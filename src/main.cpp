#define MINIAUDIO_IMPLEMENTATION
#include "platform/miniaudio.h"
#include <iostream>
#include <conio.h>

#include "midi/MIDIHandler.h"
#include "midi/MidiQueue.h"
#include "engine/Synth.h"
#include "engine/Presets.h"
#include "input/ComputerKeyboard.h"
#include "render/Render.h"
#include <thread>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <algorithm>


// instantiate queue with power-of-two size
static MidiRing<4096> gMidiQueue; // bigger to be safe

// separate SPSC queue: the main thread is its producer, RtMidi owns gMidiQueue
static MidiRing<256> gKeyQueue;

Synth synth;

// apply one event on the audio thread
static void handleEvent(const MidiEvent& e)
{
    switch (e.type) {
        case MidiEventType::NOTE_ON:
            synth.noteOn((int)e.note, (float)e.vel / 127.0f);
            break;
        case MidiEventType::NOTE_OFF:
            synth.noteOff((int)e.note);
            break;
        case MidiEventType::PROGRAM_CHANGE:
            if (e.note < NUM_PRESETS)
                synth.applyPatch(PRESETS[e.note]);
            break;
    }
}

// ------------------ AUDIO CALLBACK ------------------
void audioCallback(ma_device* device, void* pOutput, const void*, ma_uint32 frameCount)
{
    float* out = (float*)pOutput;
    float sr = device->sampleRate;

    // 1) Drain event queues first (apply events on audio thread)
    MidiEvent e;
    while (gMidiQueue.pop(e)) handleEvent(e);
    while (gKeyQueue.pop(e))  handleEvent(e);

    // 2) Render audio block
    for (ma_uint32 i = 0; i < frameCount; i++) {
        Sample s = synth.process(sr);
        out[i * 2 + 0] = s.left;
        out[i * 2 + 1] = s.right;
    }
}


// ------------------ HEADLESS RENDER ------------------
static int runRender(const char* path, int preset)
{
    constexpr unsigned SAMPLE_RATE = 48000;
    synth.prepare((float)SAMPLE_RATE);
    synth.applyPatch(PRESETS[preset]);

    auto start = std::chrono::steady_clock::now();
    std::vector<float> audio = renderSequence(synth, (float)SAMPLE_RATE, demoSequence(), 3.0);
    double renderSec = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    float peak = 0.0f;
    for (float s : audio) {
        if (!std::isfinite(s)) {
            std::cout << "Render produced a non-finite sample.\n";
            return 1;
        }
        peak = std::max(peak, std::fabs(s));
    }

    if (!writeWav(path, audio, SAMPLE_RATE)) {
        std::cout << "Failed to write " << path << "\n";
        return 1;
    }

    double audioSec = (audio.size() / 2) / (double)SAMPLE_RATE;
    std::printf("Wrote %s (%s): %.2fs at %u Hz, peak %.3f, rendered in %.3fs (%.1fx realtime)\n",
                path, PRESETS[preset].name, audioSec, SAMPLE_RATE, peak, renderSec, audioSec / renderSec);
    return 0;
}

// ------------------ MAIN ------------------
int main(int argc, char** argv)
{
    // Init lookup tables
    initDSPTables();
    initLFOShape();

    const char* renderPath = nullptr;
    int preset = 0;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--render") == 0 && i + 1 < argc) {
            renderPath = argv[++i];
        } else if (std::strcmp(argv[i], "--preset") == 0 && i + 1 < argc) {
            preset = std::atoi(argv[++i]) - 1;
        } else {
            std::printf("Usage: Synth [--preset 1-%d] [--render out.wav]\n", NUM_PRESETS);
            return 1;
        }
    }

    if (preset < 0 || preset >= NUM_PRESETS) {
        std::printf("Preset must be 1-%d\n", NUM_PRESETS);
        return 1;
    }

    if (renderPath)
        return runRender(renderPath, preset);

    synth.applyPatch(PRESETS[preset]);

    // IMPORTANT: wire MIDI → synth
    // (constructed here, not globally, so --render doesn't open a MIDI port)
    MIDIHandler midi;
    midi.synth = &synth;
    midi.setQueue(&gMidiQueue); // send events into queue

    // Audio setup
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;
    config.playback.channels = 2;
    config.dataCallback      = audioCallback;

    ma_device device;
    if (ma_device_init(NULL, &config, &device) != MA_SUCCESS) {
        std::cout << "Failed to init audio.\n";
        return -1;
    }

    // device is stopped until ma_device_start, so this can't race the callback
    synth.prepare((float)device.sampleRate);

    ma_device_start(&device);
    std::printf("Preset %d: %s\n", preset + 1, PRESETS[preset].name);

    ComputerKeyboard keyboard(gKeyQueue);
    if (keyboard.open()) {
        ComputerKeyboard::printHelp();
        while (keyboard.poll(5)) {}
    } else {
        // not an interactive console: MIDI only
        std::cout << "Press ENTER to quit.\n";
        while (true) {
            if (_kbhit() && _getch() == '\r')
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    ma_device_uninit(&device);
    return 0;
}
