#include "render/Render.h"

#include <algorithm>

#include "engine/Synth.h"
#include "platform/miniaudio.h"

std::vector<NoteEvent> demoSequence()
{
    constexpr double BAR  = 2.0;   // 120 bpm, 4/4
    constexpr double STEP = 0.25;  // eighth notes

    struct Chord { int bass; int tones[3]; };
    const Chord progression[] = {
        { 45, { 57, 60, 64 } },   // Am
        { 41, { 53, 57, 60 } },   // F
        { 48, { 55, 60, 64 } },   // C
        { 43, { 55, 59, 62 } },   // G
    };

    std::vector<NoteEvent> ev;
    auto play = [&](double t, double len, int note, float vel) {
        ev.push_back({ t, note, vel });
        ev.push_back({ t + len, note, 0.0f });
    };

    for (int pass = 0; pass < 2; ++pass) {
        for (int bar = 0; bar < 4; ++bar) {
            const Chord& c = progression[bar];
            double t0 = (pass * 4 + bar) * BAR;

            play(t0, BAR * 0.9, c.bass, 0.9f);
            for (int n : c.tones)
                play(t0, BAR * 0.85, n, 0.6f);

            // arpeggio an octave up on the second pass
            if (pass == 1) {
                const int pattern[] = { 0, 1, 2, 1, 0, 1, 2, 1 };
                for (int s = 0; s < 8; ++s)
                    play(t0 + s * STEP, STEP * 0.8, c.tones[pattern[s]] + 12, 0.7f);
            }
        }
    }
    return ev;
}

std::vector<float> renderSequence(Synth& synth, float sampleRate,
                                  std::vector<NoteEvent> events, double tailSec)
{
    // note offs before note ons at the same time, so retriggers work
    std::stable_sort(events.begin(), events.end(), [](const NoteEvent& a, const NoteEvent& b) {
        if (a.timeSec != b.timeSec) return a.timeSec < b.timeSec;
        return (a.velocity <= 0.0f) && (b.velocity > 0.0f);
    });

    double endSec = events.empty() ? 0.0 : events.back().timeSec;
    size_t totalFrames = size_t((endSec + tailSec) * sampleRate);

    std::vector<float> out(totalFrames * 2);
    size_t next = 0;

    for (size_t i = 0; i < totalFrames; ++i) {
        while (next < events.size() && size_t(events[next].timeSec * sampleRate) <= i) {
            const NoteEvent& e = events[next++];
            if (e.velocity > 0.0f) synth.noteOn(e.note, e.velocity);
            else                   synth.noteOff(e.note);
        }

        Sample s = synth.process(sampleRate);
        out[i * 2 + 0] = s.left;
        out[i * 2 + 1] = s.right;
    }
    return out;
}

bool writeWav(const char* path, const std::vector<float>& interleaved, unsigned sampleRate)
{
    ma_encoder_config config = ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, 2, sampleRate);
    ma_encoder encoder;
    if (ma_encoder_init_file(path, &config, &encoder) != MA_SUCCESS)
        return false;

    ma_uint64 frames = interleaved.size() / 2;
    ma_uint64 written = 0;
    ma_result result = ma_encoder_write_pcm_frames(&encoder, interleaved.data(), frames, &written);
    ma_encoder_uninit(&encoder);

    return result == MA_SUCCESS && written == frames;
}
