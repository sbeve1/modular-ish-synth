#include "VoiceModulation.h"
#include "Voice.h" // NOW we can include the full definition

void VoiceModulation::registerSources(Voice& v) {
    v.mod.clearAll();
    v.mod.registerSource(ModSourceID::ENV1, &v.env1);
    v.mod.registerSource(ModSourceID::LFO1, &v.lfo1);
    v.mod.registerSource(ModSourceID::NOTE, &v.noteMod);
    v.mod.registerSource(ModSourceID::VELOCITY, &v.velocityMod);
    v.mod.registerSource(ModSourceID::MOD_WHEEL, &v.modWheelMod);
    v.mod.registerSource(ModSourceID::AFTERTOUCH, &v.aftertouchMod);
    v.mod.registerSource(ModSourceID::PITCH_BEND, &v.pitchBendMod);
    v.mod.registerSource(ModSourceID::ALTRNATE, &v.alternateMod);
    v.mod.registerSource(ModSourceID::RANDOM_PER_VOICE, &v.randomVoiceMod);
    v.mod.registerSource(ModSourceID::RANDOM_PER_NOTE, &v.randomNoteMod);
}

void VoiceModulation::registerTargets(Voice& v) {
    v.registerFilterTargets(v.filter0Params, {
        { ModTargetID::FILTER0_CUTOFF, &FilterParams::cutoffMod, &FilterParams::cutoffBase },
        { ModTargetID::FILTER0_RES, &FilterParams::resMod, &FilterParams::resBase }
    });

    v.registerOscTargets(v.oscAParams, {
        { ModTargetID::OSC_A_LEVEL, &OscParams::levelMod, &OscParams::levelBase }
    });

    v.registerNoiseTargets(v.noiseParams, {
        { ModTargetID::NOISE_LEVEL, &NoiseParams::levelMod, &NoiseParams::levelBase }
    });

    v.mod.registerTarget(
        ModTargetID::BUS1_SEND,
        &v.routing.sources[0].bus1Send,
        0.0f, 0.0f, 1.0f
    );

    v.mod.registerTarget(
        ModTargetID::BUS2_SEND,
        &v.routing.sources[0].bus2Send,
        0.0f, 0.0f, 1.0f
    );

}