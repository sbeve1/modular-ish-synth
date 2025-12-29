#pragma once
// Simple multi-mode Reverb FX header.
// This FX exposes a Controls struct which the host/UI should populate (copy from engine params).
// The FX internally does not depend on engine/Params headers (keeps separation).

#include "FX.h"
#include "../../core/Sample.h"
#include <vector>
#include <cmath>
#include <atomic>


class ReverbFX : public FX {
public:

    enum class Mode : int {
        OFF = -1,
        PLATE = 0,
        HALL = 1,
        VINTAGE = 2,
        NITROUS = 3,
        BASIN = 4
    };

    // Minimal parameter fragments (mirror of engine params but intentionally local to FX)
    struct SizeParam { float base = 0.6f; float mod = 0.0f; };
    struct PreDelayParam { float base = 0.02f; float mod = 0.0f; };
    struct DampParam { float base = 0.3f; float mod = 0.0f; };
    struct WidthParam { float base = 1.0f; float mod = 0.0f; };
    struct LowCutParam  { float base = 0.0f; float mod = 0.0f; }; // 0..1
    struct HighCutParam { float base = 1.0f; float mod = 0.0f; }; // 0..1
    struct MixParam { float base = 0.2f; float mod = 0.0f; };
    struct DecayParam { float base = 2.5f; float mod = 0.0f; };
    struct SpinRateParam { float base = 0.3f; float mod = 0.0f; };
    struct SpinDepthParam { float base = 0.2f; float mod = 0.0f; };
    struct ERSizeParam { float base = 0.025f; float mod = 0.0f; };
    struct DiffusionParam { float base = 0.5f; float mod = 0.0f; };
    struct FeedbackParam { float base = 0.0f; float mod = 0.0f; };
    struct ChorusParam { float rateBase = 0.0f; float rateMod = 0.0f; float depthBase = 0.0f; float depthMod = 0.0f; };

    struct Controls {
        Mode mode = Mode::PLATE;

        // shared/simple
        SizeParam size;
        PreDelayParam preDelay;
        LowCutParam lowCut;
        HighCutParam highCut;
        MixParam mix;

        // Plate
        DampParam plateDamp;
        WidthParam plateWidth;

        // Hall
        DecayParam hallDecay;
        SpinRateParam hallSpinRate;
        SpinDepthParam hallSpinDepth;

        // Vintage
        ERSizeParam vintageERSize;
        DampParam vintageDamp;
        DiffusionParam vintageDiffA;
        DiffusionParam vintageDiffB;
        ChorusParam vintageChorus;

        // Nitrous
        DiffusionParam nitrousDiff;
        FeedbackParam nitrousFeedback;
        int nitrousModeIndex = 0; // 0..4
        ChorusParam nitrousChorus;

        // Basin
        FeedbackParam basinFeedback;
        ChorusParam basinChorus;
    };

    explicit ReverbFX(float sampleRate = 44100.0f);
    ~ReverbFX();

    // host should call this to set/replace controls atomically
    void setControls(const Controls& c);

    // FX interface
    Sample processWet(const Sample& in, float sr) override;

    // small helper to set mode quickly
    void setMode(Mode m) { controls.mode = m; }

    void registerModTargets(ModMatrix& mod) override;

    void getAvailableModTargets(std::vector<ModTargetID>& out) const;


private:
    // ---------------------
    // Helper OnePole defined first (used by members)
    // ---------------------
    struct OnePole {
        float a = 0.0f;
        float z = 0.0f;

        void setCutoff(float fc, float sr) {
            if (fc <= 0.0f) { a = 0.0f; return; }
            float x = std::expf(-2.0f * float(M_PI) * fc / sr);
            a = 1.0f - x;
        }

        inline float process(float x) {
            z += a * (x - z);
            return z;
        }

        inline void reset() { z = 0.0f; }
    };

    // ---------------------
    // State + parameters
    // ---------------------
    Controls controls;

    // internal DSP state (kept private)
    float sr = 44100.0f;
    float lfoPhase = 0.0f;
    float hallLfoPhase = 0.0f;
    float vintageChorusPhase = 0.0f;
    float nitrousChorusPhase = 0.0f;
    float basinChorusPhase = 0.0f;

    // filters for output shaping & feedback damping
    OnePole lpL, lpR;   // high-cut (low-pass)
    OnePole hpL, hpR;   // low-cut (high-pass helper)
    OnePole plateDampL, plateDampR;
    OnePole hallDampL, hallDampR;
    OnePole basinLowpassL, basinLowpassR;

    // Simple delay buffers used as placeholders for real reverb building blocks
    std::vector<float> delayL;
    std::vector<float> delayR;
    size_t idxL = 0;
    size_t idxR = 0;

    // ---- Plate (combs + allpasses) ----
    std::vector<std::vector<float>> plateCombs;
    std::vector<size_t> plateCombIndex;
    std::vector<std::vector<float>> plateAllpasses;
    std::vector<size_t> plateAllpassIndex;
    float plateAllpassGain = 0.5f;

    // ---- Hall ----
    std::vector<std::vector<float>> hallCombs;
    std::vector<size_t> hallCombIndex;
    std::vector<std::vector<float>> hallAllpass;
    std::vector<size_t> hallAllpassIndex;

    // ---- Vintage ER/diffusion ----
    std::vector<float> vintageERL, vintageERR;
    std::vector<float> vintageERTaps;
    std::vector<std::vector<float>> vintageDiffL, vintageDiffR;
    std::vector<size_t> vintageDiffIndex;

    // ---- FDN (Nitrous) ----
    std::vector<std::vector<float>> fdnDelays;
    std::vector<size_t> fdnIndex;
    std::vector<size_t> fdnBufLens;
    std::vector<std::vector<float>> fdnMatrices;

    // ---- Basin tank ----
    std::vector<float> basinDelay;
    size_t basinIndex = 0;

    // ---- Chorus / misc ----
    std::vector<float> chorusL;
    std::vector<float> chorusR;
    size_t chorusIdx = 0;

    void resetState();
    void initFDNMatrixPresets();

    // internal helpers
    Sample processByMode(const Sample& in);
    Sample processPlate(const Sample& in);
    Sample processHall(const Sample& in);
    Sample processVintage(const Sample& in);
    Sample processNitrous(const Sample& in);
    Sample processBasin(const Sample& in);

    // utility
    inline float clampf(float v, float a, float b) { return std::max(a, std::min(b, v)); }

    // a very small (safe) saturator
    inline float smallSat(float x) {
        // soft clip for safety, keep it subtle
        if (x > 1.0f) return 1.0f - 1.0f/(1.0f + (x-1.0f));
        if (x < -1.0f) return -1.0f + 1.0f/(1.0f - (x+1.0f));
        return x;
    }
};
