#define MINIAUDIO_IMPLEMENTATION
#include "platform/miniaudio.h"

#include "host/AudioHost.h"

#include <algorithm>
#include <memory>

#include "RtAudio.h"

#include "engine/Instrument.h"

struct AudioHost::Impl {
    ma_device device{};
    bool initialized = false;
    std::unique_ptr<RtAudio> asio;
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

// ASIO callback: non-interleaved buffers, so the engine renders straight into them.
static int asioCallback(void* out, void*, unsigned int frames, double, RtAudioStreamStatus, void* user)
{
    auto* impl = (AudioHost::Impl*)user;
    float* l = (float*)out;
    impl->inst->process(l, l + frames, (int)frames);
    return 0;
}

const std::vector<std::string>& AudioHost::asioDevices(bool rescan)
{
    if ((asioProbed && !rescan) || activeDriver == Driver::Asio) return asioNames;
    asioNames.clear();
    asioIds.clear();
    RtAudio probe(RtAudio::WINDOWS_ASIO, [](RtAudioErrorType, const std::string&) {});
    probe.showWarnings(false);
    for (unsigned id : probe.getDeviceIds()) {
        RtAudio::DeviceInfo info = probe.getDeviceInfo(id);
        if (info.outputChannels < 2) continue;
        asioIds.push_back(id);
        asioNames.push_back(info.name);
    }
    asioProbed = true;
    return asioNames;
}

int AudioHost::preferredAsioDevice()
{
    const auto& names = asioDevices();
    for (size_t i = 0; i < names.size(); ++i)
        if (names[i].find("Focusrite USB") != std::string::npos) return (int)i;
    for (size_t i = 0; i < names.size(); ++i)
        if (names[i].find("Focusrite") != std::string::npos) return (int)i;
    return names.empty() ? -1 : 0;
}

bool AudioHost::startAsio(Instrument& inst, int device, unsigned frames)
{
    asioDevices();   // probe before anything is opened
    stop();
    instrument = &inst;
    impl->inst = &inst;
    asioIndex = device;
    asioFrames = frames;
    lastError.clear();
    if (device < 0 || device >= (int)asioIds.size()) { lastError = "No ASIO device selected"; return false; }

    impl->asio = std::make_unique<RtAudio>(RtAudio::WINDOWS_ASIO, [](RtAudioErrorType, const std::string&) {});
    RtAudio& a = *impl->asio;
    a.showWarnings(false);
    RtAudio::DeviceInfo info = a.getDeviceInfo(asioIds[device]);
    unsigned sr = info.currentSampleRate ? info.currentSampleRate : (info.preferredSampleRate ? info.preferredSampleRate : 48000);

    RtAudio::StreamParameters out;
    out.deviceId = asioIds[device];
    out.nChannels = 2;
    out.firstChannel = 0;
    RtAudio::StreamOptions opts;
    opts.flags = RTAUDIO_NONINTERLEAVED;
    opts.streamName = "modular_synth";
    unsigned bufferSize = frames;
    if (a.openStream(&out, nullptr, RTAUDIO_FLOAT32, sr, &bufferSize, &asioCallback, impl, &opts) != RTAUDIO_NO_ERROR) {
        lastError = "ASIO: " + a.getErrorText();
        impl->asio.reset();
        return false;
    }

    // stream is open but not started: safe to prepare the engine here
    inst.prepare((float)sr, Impl::MAX_FRAMES);
    rate = sr;
    period = bufferSize;
    long lat = a.getStreamLatency();
    bufferFrames = lat > 0 ? (unsigned)lat : bufferSize;
    name = info.name;
    backend = "ASIO";
    activeDriver = Driver::Asio;

    if (a.startStream() != RTAUDIO_NO_ERROR) {
        lastError = "ASIO: " + a.getErrorText();
        stop();
        return false;
    }
    isRunning = true;
    return true;
}

bool AudioHost::start(Instrument& inst, int periodMs, bool exclusiveMode)
{
    stop();
    impl->inst = &inst;
    instrument = &inst;
    activeDriver = Driver::Wasapi;
    reqPeriod = periodMs;
    exclusive = exclusiveMode;
    lastError.clear();

    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format = ma_format_f32;
    cfg.playback.channels = 2;
    cfg.sampleRate = 0;                       // device native rate
    cfg.periodSizeInMilliseconds = (ma_uint32)periodMs;
    cfg.periods = 2;                          // default is 3: one less period queued = less latency
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
    bufferFrames = period * impl->device.playback.internalPeriods;
#ifdef MA_HAS_WASAPI
    if (impl->device.pContext->backend == ma_backend_wasapi && impl->device.wasapi.actualBufferSizeInFramesPlayback > 0)
        bufferFrames = impl->device.wasapi.actualBufferSizeInFramesPlayback;
#endif
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
    if (impl->asio) {
        if (impl->asio->isStreamRunning()) impl->asio->stopStream();
        if (impl->asio->isStreamOpen()) impl->asio->closeStream();
        impl->asio.reset();
    }
    if (impl->initialized) {
        ma_device_uninit(&impl->device);
        impl->initialized = false;
    }
    isRunning = false;
}
