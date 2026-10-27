#define MINIAUDIO_IMPLEMENTATION
#include "platform/miniaudio.h"
#include <iostream>
#include <conio.h>

#include "midi/MIDIHandler.h"
#include "midi/MidiQueue.h"
#include "engine/Synth.h"
#include "render/Render.h"
#include <thread>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <algorithm>


// instantiate queue with power-of-two size
static MidiRing<4096> gMidiQueue; // bigger to be safe

Synth synth;

// ------------------ AUDIO CALLBACK ------------------
void audioCallback(ma_device* device, void* pOutput, const void*, ma_uint32 frameCount)
{
    float* out = (float*)pOutput;
    float sr = device->sampleRate;

    // 1) Drain MIDI queue first (apply events on audio thread)
    MidiEvent e;
    while (gMidiQueue.pop(e)) {
        if (e.type == MidiEventType::NOTE_ON) {
            synth.noteOn((int)e.note, (float)e.vel / 127.0f);
        } else if (e.type == MidiEventType::NOTE_OFF) {
            synth.noteOff((int)e.note);
        }
    }

    // 2) Render audio block
    for (ma_uint32 i = 0; i < frameCount; i++) {
        Sample s = synth.process(sr);
        out[i * 2 + 0] = s.left;
        out[i * 2 + 1] = s.right;
    }
}


// ------------------ HEADLESS RENDER ------------------
static int runRender(const char* path)
{
    constexpr unsigned SAMPLE_RATE = 48000;
    synth.prepare((float)SAMPLE_RATE);

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
    std::printf("Wrote %s: %.2fs at %u Hz, peak %.3f, rendered in %.3fs (%.1fx realtime)\n",
                path, audioSec, SAMPLE_RATE, peak, renderSec, audioSec / renderSec);
    return 0;
}

// ------------------ MAIN ------------------
int main(int argc, char** argv)
{
    // Init lookup tables
    initDSPTables();
    initLFOShape();

    // PRETEND UI STATE
    // synth.setSourceEnabled(SourceID::NOISE, true);
    // synth.oscAParams.wtPosBase = 1.0f; // wavetable is on square

    if (argc >= 2 && std::strcmp(argv[1], "--render") == 0) {
        if (argc < 3) {
            std::cout << "Usage: Synth --render out.wav\n";
            return 1;
        }
        return runRender(argv[2]);
    }

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
    std::cout << "Press ENTER to quit.\n";

    // keep main thread alive; RtMidi callback is delivering MIDI
    while (true) {
        if (_kbhit() && _getch() == '\r')
            break;
        // sleep a little to reduce CPU usage on main thread
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    ma_device_uninit(&device);
    return 0;
}
