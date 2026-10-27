#pragma once

#include "audio/FX/FX.h"
#include "engine/ModMatrix.h"
#include "engine/ModTargetID.h"
#include "core/Sample.h"

#include <vector>

class ReverbFX : public FX {
public:
    // =========================
    // Modes
    // =========================
    enum class Mode : int {
        PLATE = 0,
        HALL,
        VINTAGE,
        NITROUS,
        BASIN
    };

    // =========================
    // Global Reverb Params
    // =========================
    Mode  mode = Mode::PLATE;

    float sizeBase     = 0.6f;
    float sizeMod      = 0.0f;

    float preDelayBase = 0.02f;
    float preDelayMod  = 0.0f;

    float lowCutBase   = 0.0f;
    float lowCutMod    = 0.0f;

    float highCutBase  = 1.0f;
    float highCutMod   = 0.0f;

    // =========================
    // Plate
    // =========================
    float plateDampBase  = 0.25f;
    float plateDampMod   = 0.0f;

    float plateWidthBase = 0.4f;
    float plateWidthMod  = 0.0f;

    // =========================
    // Hall
    // =========================
    float hallDecayBase     = 2.5f;
    float hallDecayMod      = 0.0f;

    float hallSpinRateBase  = 0.3f;
    float hallSpinRateMod   = 0.0f;

    float hallSpinDepthBase = 0.2f;
    float hallSpinDepthMod  = 0.0f;

    // =========================
    // Vintage
    // =========================
    float vintageERSizeBase = 0.025f;
    float vintageERSizeMod  = 0.0f;

    float vintageDampBase   = 0.3f;
    float vintageDampMod    = 0.0f;

    float vintageDiffABase  = 0.5f;
    float vintageDiffAMod   = 0.0f;

    float vintageDiffBBase  = 0.5f;
    float vintageDiffBMod   = 0.0f;

    float vintageChorusRateBase  = 0.0f;
    float vintageChorusRateMod   = 0.0f;

    float vintageChorusDepthBase = 0.0f;
    float vintageChorusDepthMod  = 0.0f;

    // =========================
    // Nitrous
    // =========================
    float nitrousDiffusionBase = 0.5f;
    float nitrousDiffusionMod  = 0.0f;

    float nitrousFeedbackBase  = 0.0f;
    float nitrousFeedbackMod   = 0.0f;

    float nitrousChorusRateBase  = 0.0f;
    float nitrousChorusRateMod   = 0.0f;

    float nitrousChorusDepthBase = 0.0f;
    float nitrousChorusDepthMod  = 0.0f;

    // =========================
    // Basin
    // =========================
    float basinFeedbackBase = 0.0f;
    float basinFeedbackMod  = 0.0f;

    float basinChorusRateBase  = 0.0f;
    float basinChorusRateMod   = 0.0f;

    float basinChorusDepthBase = 0.0f;
    float basinChorusDepthMod  = 0.0f;

    // =========================
    // Lifecycle
    // =========================
    explicit ReverbFX(float sampleRate);

    Sample processWet(const Sample& in, float sr) override;
    void registerModTargets(ModMatrix& mod) override;

private:
    float sr;

    // =========================
    // Plate DSP (ONLY IMPLEMENTED MODE)
    // =========================
    struct Comb {
        std::vector<float> buf;
        size_t idx = 0;
        float feedback = 0.8f;
        float process(float x);
    };

    struct Allpass {
        std::vector<float> buf;
        size_t idx = 0;
        float gain = 0.7f;
        float process(float x);
    };

    std::vector<Comb>    combL, combR;
    std::vector<Allpass> apL, apR;

    // =========================
    // Dispatch
    // =========================
    Sample processByMode(const Sample& in);
    Sample processPlate(const Sample& in);
};
