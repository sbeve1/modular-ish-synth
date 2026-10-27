#include "host/MidiHost.h"

#include <algorithm>

#include "engine/Instrument.h"
#include "platform/RtMidi.h"

MidiHost::MidiHost(SharedState& s) : shared(s)
{
    for (auto& m : ccMap) m.store(-1);
    try {
        in = std::make_unique<rt::midi::RtMidiIn>();
    } catch (...) {
        in.reset();
    }
}

MidiHost::~MidiHost() { close(); }

std::vector<std::string> MidiHost::ports()
{
    std::vector<std::string> out;
    if (!in) return out;
    try {
        unsigned n = in->getPortCount();
        for (unsigned i = 0; i < n; ++i) out.push_back(in->getPortName(i));
    } catch (...) {}
    return out;
}

int MidiHost::preferredPort()
{
    auto names = ports();
    for (size_t i = 0; i < names.size(); ++i) {
        std::string lower = names[i];
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower.find("daw") == std::string::npos && lower.find("loopback") == std::string::npos)
            return (int)i;
    }
    return names.empty() ? -1 : 0;
}

bool MidiHost::open(int port)
{
    close();
    if (!in || port < 0) return false;
    try {
        in->openPort((unsigned)port);
        in->ignoreTypes(true, true, true);   // ignore sysex, timing, active sensing
        in->setCallback(&MidiHost::callback, this);
        current = port;
        currentName = in->getPortName((unsigned)port);
        return true;
    } catch (...) {
        current = -1;
        currentName.clear();
        return false;
    }
}

void MidiHost::close()
{
    if (in && current >= 0) {
        try {
            in->cancelCallback();
            in->closePort();
        } catch (...) {}
    }
    current = -1;
    currentName.clear();
}

int MidiHost::ccForParam(int param) const
{
    for (int cc = 0; cc < 128; ++cc)
        if (ccMap[cc].load() == param) return cc;
    return -1;
}

void MidiHost::clearMapping(int param)
{
    for (auto& m : ccMap)
        if (m.load() == param) m.store(-1);
}

void MidiHost::callback(double, std::vector<unsigned char>* msg, void* user)
{
    if (msg && !msg->empty())
        static_cast<MidiHost*>(user)->handle(*msg);
}

void MidiHost::handle(const std::vector<unsigned char>& m)
{
    events.fetch_add(1, std::memory_order_relaxed);
    const unsigned char status = m[0] & 0xF0;
    const unsigned char d1 = m.size() > 1 ? m[1] & 0x7F : 0;
    const unsigned char d2 = m.size() > 2 ? m[2] & 0x7F : 0;

    Event e{};
    switch (status) {
        case 0x90: e.type = d2 ? EventType::NoteOn : EventType::NoteOff; e.a = d1; e.b = d2; break;
        case 0x80: e.type = EventType::NoteOff; e.a = d1; break;
        case 0xE0: e.type = EventType::PitchBend; e.f = (((int)d2 << 7 | d1) - 8192) / 8192.0f; break;
        case 0xD0: e.type = EventType::Aftertouch; e.f = d1 / 127.0f; break;
        case 0xA0: e.type = EventType::Aftertouch; e.f = d2 / 127.0f; break;   // poly AT -> channel AT
        case 0xC0: program.store(d1); return;
        case 0xB0: {
            int learn = learnParam.load();
            if (learn >= 0 && d1 != 64 && d1 != 120 && d1 != 123) {
                for (auto& slot : ccMap) if (slot.load() == learn) slot.store(-1);
                ccMap[d1].store(learn);
                learnParam.store(-1);
            }
            int p = ccMap[d1].load();
            if (p >= 0) {
                shared.params.setNorm((P)p, d2 / 127.0f);
                return;
            }
            e.type = EventType::ControlChange; e.a = d1; e.b = d2;
            break;
        }
        default: return;
    }
    shared.midiEvents.push(e);
}
