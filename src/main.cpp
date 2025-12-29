#define MINIAUDIO_IMPLEMENTATION
#include "platform/miniaudio.h"
#include <iostream>
#include <conio.h>

#include "midi/MIDIHandler.h"
#include "midi/MidiQueue.h"
#include "engine/Synth.h"
#include <thread>
#include <chrono>


// instantiate queue with power-of-two size
static MidiRing<4096> gMidiQueue; // bigger to be safe

Synth synth;
MIDIHandler midi;

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


// ------------------ MAIN ------------------
int main()
{
    // Init lookup tables
    initDSPTables();
    initLFOShape();

    // PRETEND UI STATE
    synth.setSourceEnabled(SourceID::NOISE, true);
    synth.oscAParams.wtPosBase = 1.0f; // wavetable is on square







    // IMPORTANT: wire MIDI → synth
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
