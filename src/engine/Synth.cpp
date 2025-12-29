#include "Synth.h"
#include <limits>

#include "audio/FX/ReverbFX.h"
#include "audio/FX/DelayFX.h"
#include "audio/FX/FXUtils.h"

Synth::Synth()
{   
    basicWavetable = WavetableFactory::makeBasicShapes();
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
    constexpr float VOICE_GAIN = 0.353553f;
    constexpr float MASTER_GAIN = 0.7f;
    constexpr float FX_RETURN_GAIN = 1.0f;

    fxMod.process(sr);

    Sample main{};
    Sample direct{};
    Sample bus1In{};
    Sample bus2In{};

    // ----------------------------
    // COUNT ACTIVE VOICES
    // ----------------------------
    int activeVoices = 0;
    for (auto& v : voices)
        if (v.state.active)
            activeVoices++;

    for (auto& v : voices) {
        if (!v.state.active) continue;

        VoiceOutput vo = v.process(sr);

        main.left  += vo.main.left  * VOICE_GAIN;
        main.right += vo.main.right * VOICE_GAIN;

        direct.left  += vo.direct.left  * VOICE_GAIN;
        direct.right += vo.direct.right * VOICE_GAIN;

        bus1In.left  += vo.bus1.left  * VOICE_GAIN * SERUM_FX_SEND_GAIN;
        bus1In.right += vo.bus1.right * VOICE_GAIN * SERUM_FX_SEND_GAIN;

        bus2In.left  += vo.bus2.left  * VOICE_GAIN * SERUM_FX_SEND_GAIN;
        bus2In.right += vo.bus2.right * VOICE_GAIN * SERUM_FX_SEND_GAIN;
    }

    // ----------------------------
    // FX ALWAYS RUN
    // ----------------------------
    Sample bus1Out = bus1FX.process(bus1In, sr);
    Sample bus2Out = bus2FX.process(bus2In, sr);

    bus1Out.left  = serumSat(bus1Out.left);
    bus1Out.right = serumSat(bus1Out.right);
    bus2Out.left  = serumSat(bus2Out.left);
    bus2Out.right = serumSat(bus2Out.right);

    main.left  += (bus1Out.left  + bus2Out.left)  * FX_RETURN_GAIN;
    main.right += (bus1Out.right + bus2Out.right) * FX_RETURN_GAIN;

    main = mainFX.process(main, sr);

    if (fabs(main.left)  > 1e-6f) main.left  = serumLimiter(main.left);
    if (fabs(main.right) > 1e-6f) main.right = serumLimiter(main.right);


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

    // (optional) register macro / global sources here later

    bus1FX.registerModTargets(fxMod);
    bus2FX.registerModTargets(fxMod);
    mainFX.registerModTargets(fxMod);
}
