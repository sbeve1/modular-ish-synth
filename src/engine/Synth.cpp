#include "Synth.h"
#include <limits>

#include "audio/fx/ReverbFX.h"


Synth::Synth()
{
    basicWavetable = WavetableFactory::makeBasicShapes();

    // TEMP: hardcode reverb on BUS1
    bus1FX.clear();
    bus1FX.add(std::make_unique<ReverbFX>(44100.0f));
}

void Synth::noteOn(int note, float velocity)
{
    Voice* chosen = nullptr;

    // 1) Find a free voice
    for (auto& v : voices) {
        if (!v.state.active) {
            chosen = &v;
            break;
        }
    }

    // 2) Steal the oldest voice if all are active
    if (!chosen) {
        int oldestId = std::numeric_limits<int>::max();
        for (auto& v : voices) {
            if (v.state.noteId < oldestId) {
                oldestId = v.state.noteId;
                chosen = &v;
            }
        }
    }

    if (!chosen)
        return;

    chosen->state.noteId = nextNoteId++;
    chosen->start(note, velocity, &basicWavetable);
}

void Synth::noteOff(int note)
{
    // Release only the most recent voice for this note
    int bestId = -1;
    Voice* chosen = nullptr;

    for (auto& v : voices) {
        if (v.state.active && v.state.midiNote == note) {
            if (!chosen || v.state.noteId > bestId) {
                bestId = v.state.noteId;
                chosen = &v;
            }
        }
    }

    if (chosen)
        chosen->stop();
}

Sample Synth::process(float sr)
{
    constexpr float VOICE_GAIN_NORM = 0.353553f; // 1/sqrt(8)
    constexpr float MASTER_GAIN    = 0.7f;

    Sample globalMain{};
    Sample globalBus1{};
    Sample globalBus2{};
    Sample globalDirect{};

    for (auto& v : voices) {
        if (!v.state.active)
            continue;

        VoiceOutput vo = v.process(sr);

        globalMain.left   += vo.main.left   * VOICE_GAIN_NORM;
        globalMain.right  += vo.main.right  * VOICE_GAIN_NORM;

        globalBus1.left   += vo.bus1.left   * VOICE_GAIN_NORM;
        globalBus1.right  += vo.bus1.right  * VOICE_GAIN_NORM;

        globalBus2.left   += vo.bus2.left   * VOICE_GAIN_NORM;
        globalBus2.right  += vo.bus2.right  * VOICE_GAIN_NORM;

        globalDirect.left += vo.direct.left * VOICE_GAIN_NORM;
        globalDirect.right+= vo.direct.right* VOICE_GAIN_NORM;
    }

    // BUS FX
    globalBus1 = bus1FX.process(globalBus1, sr);
    globalBus2 = bus2FX.process(globalBus2, sr);

    // buses → main
    globalMain.left  += globalBus1.left + globalBus2.left;
    globalMain.right += globalBus1.right + globalBus2.right;

    // MAIN FX
    globalMain = mainFX.process(globalMain, sr);

    // FINAL OUTPUT
    Sample out;
    out.left  = (globalMain.left  + globalDirect.left)  * MASTER_GAIN;
    out.right = (globalMain.right + globalDirect.right) * MASTER_GAIN;

    return out;
}

void Synth::setSourceDest(SourceID src, SourceDest dest)
{
    for (auto& v : voices) {
        RoutingController::setSourceDest(v.routing, src, dest);
    }
}

void Synth::setSourceBusSend(SourceID src, BusID bus, float amount)
{
    for (auto& v : voices) {
        RoutingController::setSourceBusSend(v.routing, src, bus, amount);
    }
}
