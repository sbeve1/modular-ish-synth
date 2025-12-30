#include "audio/FX/ReverbFX.h"
#include <algorithm>

// ------------------------------------------------------------
// DSP helpers
// ------------------------------------------------------------
float ReverbFX::Comb::process(float x)
{
    constexpr float MAX_FEEDBACK = 0.78f;

    float y = buf[idx];
    
    float fb = std::min(feedback, MAX_FEEDBACK);
    buf[idx] = x + y * fb;

    idx = (idx + 1) % buf.size();
    return y;
}

float ReverbFX::Allpass::process(float x)
{
    float y = buf[idx];
    float out = -x + y;
    buf[idx] = x + y * gain;
    idx = (idx + 1) % buf.size();
    return out;
}

// ------------------------------------------------------------
// Construction
// ------------------------------------------------------------
ReverbFX::ReverbFX(float sampleRate)
    : sr(sampleRate)
{
    wetBase = 1.0f; // send FX

    combL = {
        { std::vector<float>(size_t(sr * 0.030f)), 0, 0.82f },
        { std::vector<float>(size_t(sr * 0.037f)), 0, 0.80f },
        { std::vector<float>(size_t(sr * 0.041f)), 0, 0.78f },
        { std::vector<float>(size_t(sr * 0.043f)), 0, 0.76f },
    };

    combR = {
        { std::vector<float>(size_t(sr * 0.031f)), 0, 0.82f },
        { std::vector<float>(size_t(sr * 0.038f)), 0, 0.80f },
        { std::vector<float>(size_t(sr * 0.042f)), 0, 0.78f },
        { std::vector<float>(size_t(sr * 0.044f)), 0, 0.76f },
    };

    apL = {
        { std::vector<float>(size_t(sr * 0.005f)), 0, 0.7f },
        { std::vector<float>(size_t(sr * 0.0017f)), 0, 0.7f },
    };

    apR = {
        { std::vector<float>(size_t(sr * 0.0053f)), 0, 0.7f },
        { std::vector<float>(size_t(sr * 0.0019f)), 0, 0.7f },
    };
}

// ------------------------------------------------------------
// Processing
// ------------------------------------------------------------
Sample ReverbFX::processWet(const Sample& in, float)
{
    return processByMode(in);
}

Sample ReverbFX::processByMode(const Sample& in)
{
    switch (mode) {
        case Mode::PLATE:
            return processPlate(in);

        default:
            return {}; // future modes stubbed
    }
}

// ------------------------------------------------------------
// Plate
// ------------------------------------------------------------
Sample ReverbFX::processPlate(const Sample& in)
{   
    constexpr float REVERB_INPUT_GAIN = 0.35f;
    constexpr float REVERB_OUT_GAIN = 0.35f;

    float inL = in.left  * REVERB_INPUT_GAIN;
    float inR = in.right * REVERB_INPUT_GAIN;


    float accL = 0.0f;
    float accR = 0.0f;

    for (auto& c : combL) accL += c.process(inL) * 0.85f;

    for (auto& c : combR) accR += c.process(inR) * 0.85f;

    accL *= (1.0f / combL.size());
    accR *= (1.0f / combR.size());

    for (auto& ap : apL) accL = ap.process(accL);
    for (auto& ap : apR) accR = ap.process(accR);

    Sample out{};

    out.left  = accL * REVERB_OUT_GAIN;
    out.right = accR * REVERB_OUT_GAIN;

    return out;
}

// ------------------------------------------------------------
// Modulation Registration (FULL SURFACE)
// ------------------------------------------------------------
void ReverbFX::registerModTargets(ModMatrix& mod)
{
    mod.registerTarget(ModTargetID::FX_WET, &wetMod, wetBase, 0.0f, 1.0f);

    mod.registerTarget(ModTargetID::REVERB_SIZE,
        &sizeMod, sizeBase, 0.0f, 1.0f);

    mod.registerTarget(ModTargetID::REVERB_PREDELAY,
        &preDelayMod, preDelayBase, 0.0f, 0.1f);

    mod.registerTarget(ModTargetID::REVERB_LOW_CUT,
        &lowCutMod, lowCutBase, 0.0f, 1.0f);

    mod.registerTarget(ModTargetID::REVERB_HIGH_CUT,
        &highCutMod, highCutBase, 0.0f, 1.0f);

    // Plate
    mod.registerTarget(ModTargetID::REVERB_PLATE_DAMP,
        &plateDampMod, plateDampBase, 0.0f, 1.0f);

    mod.registerTarget(ModTargetID::REVERB_PLATE_WIDTH,
        &plateWidthMod, plateWidthBase, 0.0f, 1.0f);

    // Other modes intentionally registered but unused
    mod.registerTarget(ModTargetID::REVERB_HALL_DECAY,
        &hallDecayMod, hallDecayBase, 0.1f, 10.0f);

    mod.registerTarget(ModTargetID::REVERB_VINTAGE_ER_SIZE,
        &vintageERSizeMod, vintageERSizeBase, 0.0f, 0.1f);

    mod.registerTarget(ModTargetID::REVERB_NITROUS_FEEDBACK,
        &nitrousFeedbackMod, nitrousFeedbackBase, 0.0f, 1.0f);

    mod.registerTarget(ModTargetID::REVERB_BASIN_FEEDBACK,
        &basinFeedbackMod, basinFeedbackBase, 0.0f, 1.0f);
}
