#include "Synth.h"
#include <limits>
#include <cmath>

#include "audio/FX/ReverbFX.h"
#include "audio/FX/DelayFX.h"
#include "audio/FX/FXUtils.h"

Synth::Synth()
{
    basicWavetable = WavetableFactory::makeBasicShapes();

    // ----------------------------
    // FX CHAINS
    // ----------------------------
    bus1FX.clear();
    bus1FX.add(std::make_unique<ReverbFX>(44100.0f));

    bus2FX.clear();
    // bus2FX.add(std::make_unique<DelayFX>(44100.0f)); // optional

    mainFX.clear();
    mainFX.add(std::make_unique<ReverbFX>(44100.0f));
}

void Synth::noteOn(int note, float velocity)
{
    Voice* chosen = nullptr;

    for (auto& v : voices) {
        if (!v.state.active) {
            chosen = &v;
            break;
        }
    }

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

    chosen->oscAParams = oscAParams;
    chosen->oscBParams = oscBParams;
    chosen->oscCParams = oscCParams;
    chosen->subParams  = subParams;
    chosen->noiseParams = noiseParams;

    chosen->oscAParams.midiNote = note;
    chosen->oscBParams.midiNote = note;
    chosen->oscCParams.midiNote = note;
    chosen->subParams.midiNote  = note;
}

void Synth::noteOff(int note)
{
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
    constexpr float VOICE_GAIN  = 0.353553f;
    constexpr float MASTER_GAIN = 0.7f;

    fxMod.process(sr);

    // ----------------------------
    // BUS ACCUMULATORS
    // ----------------------------
    Sample main{};
    Sample bus1{};
    Sample bus2{};
    Sample direct{};

    // ----------------------------
    // VOICE SUMMING
    // ----------------------------
    for (auto& v : voices) {
        if (!v.state.active) continue;

        VoiceOutput vo = v.process(sr);

        main.left  += vo.main.left  * VOICE_GAIN;
        main.right += vo.main.right * VOICE_GAIN;

        direct.left  += vo.direct.left  * VOICE_GAIN;
        direct.right += vo.direct.right * VOICE_GAIN;

        bus1.left  += vo.bus1.left  * VOICE_GAIN * SERUM_FX_SEND_GAIN;
        bus1.right += vo.bus1.right * VOICE_GAIN * SERUM_FX_SEND_GAIN;

        bus2.left  += vo.bus2.left  * VOICE_GAIN * SERUM_FX_SEND_GAIN;
        bus2.right += vo.bus2.right * VOICE_GAIN * SERUM_FX_SEND_GAIN;
    }

    // ============================================================
    // BUS 1 FX (SNAPSHOT → FX → ADD BACK)
    // ============================================================
    {
        Sample bus1FXIn = bus1;                    // snapshot
        Sample bus1Wet  = bus1FX.process(bus1FXIn, sr);

        bus1Wet.left  = serumSat(bus1Wet.left);
        bus1Wet.right = serumSat(bus1Wet.right);

        bus1.left  += bus1Wet.left;
        bus1.right += bus1Wet.right;
    }

    // ============================================================
    // BUS 2 FX (SNAPSHOT → FX → ADD BACK)
    // ============================================================
    {
        Sample bus2FXIn = bus2;                    // snapshot
        Sample bus2Wet  = bus2FX.process(bus2FXIn, sr);

        bus2Wet.left  = serumSat(bus2Wet.left);
        bus2Wet.right = serumSat(bus2Wet.right);

        bus2.left  += bus2Wet.left;
        bus2.right += bus2Wet.right;
    }

    // ----------------------------
    // ROUTE BUSES INTO MAIN
    // ----------------------------
    main.left  += bus1.left + bus2.left;
    main.right += bus1.right + bus2.right;

    // ============================================================
    // MAIN FX (SNAPSHOT → FX → ADD BACK)
    // ============================================================
    {
        Sample mainFXIn = main;                    // snapshot
        Sample mainWet  = mainFX.process(mainFXIn, sr);

        main.left  += mainWet.left;
        main.right += mainWet.right;
    }

    // ----------------------------
    // MASTER
    // ----------------------------
    main.left  = serumLimiter(main.left);
    main.right = serumLimiter(main.right);

    Sample out{};
    out.left  = (main.left  + direct.left)  * MASTER_GAIN;
    out.right = (main.right + direct.right) * MASTER_GAIN;
    return out;
}

void Synth::setSourceEnabled(SourceID src, bool enabled)
{
    for (auto& v : voices)
        v.routing.sources[(int)src].enabled = enabled;
}

void Synth::setFilterEnabled(int filterIndex, bool enabled)
{
    for (auto& v : voices)
        v.routing.filter[filterIndex].enabled = enabled;
}

void Synth::setSourceDest(SourceID src, SourceDest dest)
{
    for (auto& v : voices)
        RoutingController::setSourceDest(v.routing, src, dest);
}

void Synth::setSourceBusSend(SourceID src, BusID bus, float amount)
{
    for (auto& v : voices)
        RoutingController::setSourceBusSend(v.routing, src, bus, amount);
}

void Synth::rebuildFXModulation()
{
    fxMod.clearAll();
    bus1FX.registerModTargets(fxMod);
    bus2FX.registerModTargets(fxMod);
    mainFX.registerModTargets(fxMod);
}
