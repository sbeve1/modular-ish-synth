#define MINIAUDIO_IMPLEMENTATION
#include "platform/miniaudio.h"

#include "host/AudioHost.h"

#include <algorithm>

#include "engine/Instrument.h"

struct AudioHost::Impl {
    ma_device device{};
    bool initialized = false;
    Instrument* inst = nullptr;
    static constexpr int MAX_FRAMES = 4096;
    float l[MAX_FRAMES], r[MAX_FRAMES];
};

static void dataCallback(ma_device* dev, void* out, const void*, ma_uint32 frames)
{
    auto* impl = (AudioHost::Impl*)dev->pUserData;
    float* o = (float*)out;
    ma_uint32 done = 0;
    while (done < frames) {
        int n = (int)std::min<ma_uint32>(frames - done, AudioHost::Impl::MAX_FRAMES);
        impl->inst->process(impl->l, impl->r, n);
        for (int i = 0; i < n; ++i) {
            o[(done + i) * 2 + 0] = impl->l[i];
            o[(done + i) * 2 + 1] = impl->r[i];
        }
        done += n;
    }
}

AudioHost::AudioHost() : impl(new Impl) {}
AudioHost::~AudioHost() { stop(); delete impl; }

bool AudioHost::start(Instrument& inst, int periodMs, bool exclusiveMode)
{
    stop();
    impl->inst = &inst;
    instrument = &inst;
    reqPeriod = periodMs;
    exclusive = exclusiveMode;
    lastError.clear();

    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format = ma_format_f32;
    cfg.playback.channels = 2;
    cfg.sampleRate = 0;                       // device native rate
    cfg.periodSizeInMilliseconds = (ma_uint32)periodMs;
    cfg.performanceProfile = ma_performance_profile_low_latency;
    cfg.playback.shareMode = exclusiveMode ? ma_share_mode_exclusive : ma_share_mode_shared;
    cfg.dataCallback = dataCallback;
    cfg.pUserData = impl;

    if (ma_device_init(nullptr, &cfg, &impl->device) != MA_SUCCESS) {
        if (!exclusiveMode) { lastError = "Could not open the audio device"; return false; }
        // exclusive mode refused (device busy or unsupported): fall back to shared
        cfg.playback.shareMode = ma_share_mode_shared;
        exclusive = false;
        if (ma_device_init(nullptr, &cfg, &impl->device) != MA_SUCCESS) {
            lastError = "Could not open the audio device";
            return false;
        }
        lastError = "Exclusive mode unavailable; using shared mode";
    }
    impl->initialized = true;

    rate = impl->device.sampleRate;
    period = impl->device.playback.internalPeriodSizeInFrames;
    name = impl->device.playback.name;
    backend = ma_get_backend_name(impl->device.pContext->backend);

    // device is stopped until ma_device_start: safe to prepare here
    inst.prepare((float)rate, Impl::MAX_FRAMES);

    if (ma_device_start(&impl->device) != MA_SUCCESS) {
        lastError = "Could not start the audio device";
        stop();
        return false;
    }
    isRunning = true;
    return true;
}

bool AudioHost::restart(int periodMs, bool exclusiveMode)
{
    if (!instrument) return false;
    return start(*instrument, periodMs, exclusiveMode);
}

void AudioHost::stop()
{
    if (impl->initialized) {
        ma_device_uninit(&impl->device);
        impl->initialized = false;
    }
    isRunning = false;
}
