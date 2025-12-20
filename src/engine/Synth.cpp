#include "Synth.h"
#include <cmath>

static inline Sample applySourceParams(const Sample& in, const SourceParams& p)
{
    Sample s;
    s.left  = in.left  * p.levelMod;
    s.right = in.right * p.levelMod;

    float a = (p.panMod * 0.5f + 0.5f) * HALF_PI;
    Sample out;
    out.left  = s.left  * cosf(a);
    out.right = s.right * sinf(a);
    return out;
}

Synth::Synth()
{
    // ----------------------------
    // CREATE DEFAULT WAVETABLE
    // ----------------------------
    basicWavetable = WavetableFactory::makeBasicShapes();

    // ----------------------------
    // DEFAULT ROUTING
    // ----------------------------
    router.oscA = { 0.f, 1.f, 0.f, 0.f };
    router.oscB = { 0.f, 1.f, 0.f, 0.f };
    router.oscC = { 0.f, 1.f, 0.f, 0.f };
    router.sub  = { 0.f, 1.f, 0.f, 0.f };
    router.noise= { 0.f, 1.f, 0.f, 0.f };

    // ----------------------------
    // DEFAULT SOURCE PARAMS
    // ----------------------------
    oscAParams.levelBase = oscAParams.levelMod = 1.0f;
    oscAParams.panBase   = oscAParams.panMod   = 0.0f;

    oscBParams = oscAParams;
    oscCParams = oscAParams;
    subParams  = oscAParams;
    noiseParams= oscAParams;
}

void Synth::noteOn(int note, float velocity)
{
    Voice* v = nullptr;
    for (auto& voice : voices) {
        if (!voice.active) {
            v = &voice;
            break;
        }
    }

    if (!v) {
        v = &voices[nextVoice];
        nextVoice = (nextVoice + 1) % NUM_VOICES;
    }

    // ----------------------------
    // PASS WAVETABLE TO VOICE
    // ----------------------------
    v->start(note, velocity, &basicWavetable);

    // Routing mod targets (unchanged)
    v->mod.registerTarget(ModTargetID::OSC_A_TO_DIRECT, &router.oscA.toDirect, router.oscA.toDirect);
    v->mod.registerTarget(ModTargetID::OSC_A_TO_MAIN,   &router.oscA.toMain,   router.oscA.toMain);
    v->mod.registerTarget(ModTargetID::OSC_A_TO_BUS1,   &router.oscA.toBus1,   router.oscA.toBus1);
    v->mod.registerTarget(ModTargetID::OSC_A_TO_BUS2,   &router.oscA.toBus2,   router.oscA.toBus2);

    v->mod.registerTarget(ModTargetID::OSC_B_TO_DIRECT, &router.oscB.toDirect, router.oscB.toDirect);
    v->mod.registerTarget(ModTargetID::OSC_B_TO_MAIN,   &router.oscB.toMain,   router.oscB.toMain);
    v->mod.registerTarget(ModTargetID::OSC_B_TO_BUS1,   &router.oscB.toBus1,   router.oscB.toBus1);
    v->mod.registerTarget(ModTargetID::OSC_B_TO_BUS2,   &router.oscB.toBus2,   router.oscB.toBus2);

    v->mod.registerTarget(ModTargetID::OSC_C_TO_DIRECT, &router.oscC.toDirect, router.oscC.toDirect);
    v->mod.registerTarget(ModTargetID::OSC_C_TO_MAIN,   &router.oscC.toMain,   router.oscC.toMain);
    v->mod.registerTarget(ModTargetID::OSC_C_TO_BUS1,   &router.oscC.toBus1,   router.oscC.toBus1);
    v->mod.registerTarget(ModTargetID::OSC_C_TO_BUS2,   &router.oscC.toBus2,   router.oscC.toBus2);

    v->mod.registerTarget(ModTargetID::SUB_TO_DIRECT, &router.sub.toDirect, router.sub.toDirect);
    v->mod.registerTarget(ModTargetID::SUB_TO_MAIN,   &router.sub.toMain,   router.sub.toMain);
    v->mod.registerTarget(ModTargetID::SUB_TO_BUS1,   &router.sub.toBus1,   router.sub.toBus1);
    v->mod.registerTarget(ModTargetID::SUB_TO_BUS2,   &router.sub.toBus2,   router.sub.toBus2);

    v->mod.registerTarget(ModTargetID::NOISE_TO_DIRECT, &router.noise.toDirect, router.noise.toDirect);
    v->mod.registerTarget(ModTargetID::NOISE_TO_MAIN,   &router.noise.toMain,   router.noise.toMain);
    v->mod.registerTarget(ModTargetID::NOISE_TO_BUS1,   &router.noise.toBus1,   router.noise.toBus1);
    v->mod.registerTarget(ModTargetID::NOISE_TO_BUS2,   &router.noise.toBus2,   router.noise.toBus2);
}

void Synth::noteOff(int note)
{
    for (auto& v : voices)
        if (v.active && v.midiNote == note)
            v.stop();
}

Sample Synth::process(float sr)
{
    Sample direct{};
    Sample main{};

    bus1.clear();
    bus2.clear();

    for (auto& v : voices)
    {
        if (!v.active) continue;

        VoiceSources src = v.processSources(sr);
        float env = v.envValue(sr);

        src.oscA  = applySourceParams(src.oscA,  oscAParams);
        src.oscB  = applySourceParams(src.oscB,  oscBParams);
        src.oscC  = applySourceParams(src.oscC,  oscCParams);
        src.sub   = applySourceParams(src.sub,   subParams);
        src.noise = applySourceParams(src.noise, noiseParams);

        RoutedSignal r = router.route(src);

        r.direct = amp.apply(r.direct, env);
        r.main   = amp.apply(r.main,   env);
        r.bus1   = amp.apply(r.bus1,   env);
        r.bus2   = amp.apply(r.bus2,   env);

        direct.left  += r.direct.left;
        direct.right += r.direct.right;

        main.left  += r.main.left;
        main.right += r.main.right;

        bus1.signal.left  += r.bus1.left;
        bus1.signal.right += r.bus1.right;

        bus2.signal.left  += r.bus2.left;
        bus2.signal.right += r.bus2.right;
    }

    Sample out;
    out.left  = direct.left  + main.left  + bus1.signal.left  + bus2.signal.left;
    out.right = direct.right + main.right + bus1.signal.right + bus2.signal.right;

    return limiter.process(out);
}
