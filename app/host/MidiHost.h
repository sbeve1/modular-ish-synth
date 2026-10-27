#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>

struct SharedState;
namespace rt { namespace midi { class RtMidiIn; } }

// MIDI input. One port at a time so the RtMidi callback stays the only
// producer for SharedState::midiEvents (an SPSC ring).
//
// MIDI learn: armLearn(param) makes the next CC move bind to that param;
// bound CCs write the parameter directly (atomics) from the MIDI thread.
class MidiHost {
public:
    explicit MidiHost(SharedState& s);
    ~MidiHost();

    std::vector<std::string> ports();
    bool open(int port);
    void close();
    int  openPort() const { return current; }
    const std::string& openPortName() const { return currentName; }
    int  preferredPort();             // a sensible default (skips "DAW" ports)

    void armLearn(int param) { learnParam.store(param); }
    int  learning() const { return learnParam.load(); }
    int  ccForParam(int param) const;
    void clearMapping(int param);
    int  mappedParam(int cc) const { return ccMap[cc & 127].load(); }

    // last program change received (-1 none); the UI consumes it
    int takeProgramChange() { return program.exchange(-1); }
    // a short activity counter for the UI's MIDI LED
    uint32_t activity() const { return events.load(); }

private:
    SharedState& shared;
    std::unique_ptr<rt::midi::RtMidiIn> in;
    int current = -1;
    std::string currentName;

    std::atomic<int> learnParam{ -1 };
    std::atomic<int> ccMap[128];
    std::atomic<int> program{ -1 };
    std::atomic<uint32_t> events{ 0 };

    static void callback(double, std::vector<unsigned char>* msg, void* user);
    void handle(const std::vector<unsigned char>& m);
};
