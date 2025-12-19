#define MINIAUDIO_IMPLEMENTATION
#include "platform/miniaudio.h"
#include <iostream>
#include <conio.h>

#include "midi/MIDIHandler.h"
#include "engine/Synth.h"

Synth synth;
MIDIHandler midi;

// ------------------ AUDIO CALLBACK ------------------
void audioCallback(ma_device* device, void* pOutput, const void*, ma_uint32 frameCount) {
    float* out = (float*)pOutput;
    float sr = device -> sampleRate;

    for (ma_uint32 i = 0; i < frameCount; i++) {
        Sample s = synth.process(sr);
        out[i * 2 + 0] = s.left;
        out[i * 2 + 1] = s.right;
    }
}

// ------------------ MAIN ------------------
int main() {
    // Audio setup
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.dataCallback = audioCallback;

    ma_device device;
    if (ma_device_init(NULL, &config, &device) != MA_SUCCESS) {
        std::cout << "Failed to init audio.\n";
        return -1;
    }
    ma_device_start(&device);

    std::cout << "Press ENTER to quit.\n";

    unsigned char status, note, vel;

    // MIDI event loop
    while (true) {
        if (midi.poll(status, note, vel)) {
            if ((status & 0xF0) == 0x90 && vel > 0)
                synth.noteOn(note, vel / 127.0f);
            if ((status & 0xF0) == 0x80 || ((status & 0xF0) == 0x90 && vel == 0))
                synth.noteOff(note);
        }

        // read for kb input and ENTER
        if (_kbhit() && _getch() == '\r') {
            break;
        }

    }

    ma_device_uninit(&device);
    return 0;
}
