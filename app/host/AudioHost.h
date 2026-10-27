#pragma once

#include <string>
#include <vector>

class Instrument;

// Owns the audio device: Windows audio (WASAPI, via miniaudio) or an ASIO
// driver (via RtAudio). The callback only calls Instrument::process (and
// deinterleaves for WASAPI); nothing else runs on the audio thread.
class AudioHost {
public:
    enum class Driver { Wasapi, Asio };

    AudioHost();
    ~AudioHost();

    // periodMs: requested buffer length; the backend may round it.
    bool start(Instrument& inst, int periodMs = 5, bool exclusive = false);
    void stop();
    bool restart(int periodMs, bool exclusive);   // same instrument, new buffer settings

    // ASIO: device list is probed once (drivers can't be probed while one is open)
    const std::vector<std::string>& asioDevices(bool rescan = false);
    int  preferredAsioDevice();                    // a Focusrite if present, else the first
    bool startAsio(Instrument& inst, int device, unsigned bufferFrames);
    bool restartAsio(int device, unsigned bufferFrames) { return instrument && startAsio(*instrument, device, bufferFrames); }
    Driver driver() const { return activeDriver; }
    int  asioDevice() const { return asioIndex; }
    unsigned requestedAsioFrames() const { return asioFrames; }

    int  requestedPeriodMs() const { return reqPeriod; }
    bool exclusiveMode() const { return exclusive; }

    bool running() const { return isRunning; }
    unsigned sampleRate() const { return rate; }
    unsigned periodFrames() const { return period; }
    unsigned outputBufferFrames() const { return bufferFrames; }   // total queued output (drives latency)
    const std::string& deviceName() const { return name; }
    const std::string& backendName() const { return backend; }
    const std::string& error() const { return lastError; }

    struct Impl;   // device state (public so the C callback can reach it)

private:
    Impl* impl;
    Instrument* instrument = nullptr;
    Driver activeDriver = Driver::Wasapi;
    int asioIndex = -1;
    unsigned asioFrames = 128;
    std::vector<std::string> asioNames;
    std::vector<unsigned> asioIds;
    bool asioProbed = false;
    int reqPeriod = 5;
    bool exclusive = false;
    bool isRunning = false;
    unsigned rate = 0, period = 0, bufferFrames = 0;
    std::string name, backend, lastError;
};
