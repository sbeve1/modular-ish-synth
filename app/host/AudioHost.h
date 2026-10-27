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
    bool start(Instrument& inst, int periodMs = 5);
    void stop();

    bool running() const { return isRunning; }
    unsigned sampleRate() const { return rate; }
    unsigned periodFrames() const { return period; }
    const std::string& deviceName() const { return name; }
    const std::string& backendName() const { return backend; }
    const std::string& error() const { return lastError; }

    struct Impl;   // device state (public so the C callback can reach it)

private:
    Impl* impl;
    bool isRunning = false;
    unsigned rate = 0, period = 0;
    std::string name, backend, lastError;
};
