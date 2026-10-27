#pragma once

#include <string>
#include <vector>

class Instrument;

// Owns the audio device. The callback only deinterleaves and calls
// Instrument::process; nothing else runs on the audio thread.
class AudioHost {
public:
    AudioHost();
    ~AudioHost();

    // periodMs: requested buffer length; the backend may round it.
    bool start(Instrument& inst, int periodMs = 5, bool exclusive = false);
    void stop();
    bool restart(int periodMs, bool exclusive);   // same instrument, new buffer settings

    int  requestedPeriodMs() const { return reqPeriod; }
    bool exclusiveMode() const { return exclusive; }

    bool running() const { return isRunning; }
    unsigned sampleRate() const { return rate; }
    unsigned periodFrames() const { return period; }
    const std::string& deviceName() const { return name; }
    const std::string& backendName() const { return backend; }
    const std::string& error() const { return lastError; }

    struct Impl;   // device state (public so the C callback can reach it)

private:
    Impl* impl;
    Instrument* instrument = nullptr;
    int reqPeriod = 5;
    bool exclusive = false;
    bool isRunning = false;
    unsigned rate = 0, period = 0;
    std::string name, backend, lastError;
};
