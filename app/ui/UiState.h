#pragma once

#include <string>
#include <vector>

#include "engine/Instrument.h"
#include "engine/Presets.h"
#include "engine/ResourceManager.h"
#include "ui/Undo.h"

class MidiHost;
class AudioHost;

// State shared by all UI code (UI thread only).
struct UiState {
    SharedState&      s;
    ResourceManager& wt;
    MidiHost&         midi;
    AudioHost&        audio;

    UiState(SharedState& s_, ResourceManager& wt_, MidiHost& m, AudioHost& a)
        : s(s_), wt(wt_), midi(m), audio(a) {}

    // pages
    enum Page { PageSynth, PageMatrix, PageFx, PageGlobal };
    int page = PageSynth;
    int envTab = 0, lfoTab = 0, filterTab = 0;
    // envelope graph view per envelope: seconds shown from 0, and whether the
    // view is locked to the release end (unlocked: the zoom stays where you set it)
    float envViewSec[3] = { 2.0f, 2.0f, 2.0f };
    bool envViewLocked[3] = { true, true, true };
    int fxSelected = 0;           // FxId shown in the FX editor

    UndoHistory undo;

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
    float cpuHistory[240] = {};
    int   cpuHistoryPos = 0;
    std::string status;           // transient footer message
    double statusUntil = 0.0;

    void setStatus(const std::string& msg, double now, double seconds = 2.5)
    {
        status = msg;
        statusUntil = now + seconds;
    }
};
