#pragma once

#include <string>
#include <vector>

#include "engine/Instrument.h"
#include "engine/Presets.h"
#include "engine/WavetableManager.h"

class MidiHost;
class AudioHost;

// State shared by all UI code (UI thread only).
struct UiState {
    SharedState&      s;
    WavetableManager& wt;
    MidiHost&         midi;
    AudioHost&        audio;

    UiState(SharedState& s_, WavetableManager& wt_, MidiHost& m, AudioHost& a)
        : s(s_), wt(wt_), midi(m), audio(a) {}

    // pages
    enum Page { PageSynth, PageMatrix, PageFx, PageGlobal };
    int page = PageSynth;
    int envTab = 0, lfoTab = 0;

    // presets
    std::vector<Preset> factory;
    std::vector<std::string> userFiles;
    std::string presetName = "Init";
    int presetIndex = 0;          // index into the combined list (factory then user)
    bool presetDirty = false;

    // modulation editing
    int lastTouchedSlot = -1;

    // wavetable editor
    bool wtEditorOpen = false;
    int  wtEditorOsc = 0;

    // keyboard input
    int keyOctave = 4;            // computer keyboard base octave (C4)
    int keyVelocity = 100;

    // visual state
    float peakL = 0.0f, peakR = 0.0f;
    std::string status;           // transient footer message
    double statusUntil = 0.0;

    void setStatus(const std::string& msg, double now, double seconds = 2.5)
    {
        status = msg;
        statusUntil = now + seconds;
    }
};
