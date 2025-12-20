#include "MIDIHandler.h"
#include "engine/Synth.h"
#include <iostream>

// ------------------------ CONSTRUCTOR ------------------------
MIDIHandler::MIDIHandler()
{
    std::cout << "RtMidi API = " << midiin.getCurrentApi() << std::endl;

    unsigned int ports = midiin.getPortCount();

    std::cout << "MIDI ports: " << ports << "\n";
    if (ports == 0) {
        std::cout << "No MIDI input devices detected.\n";
        return;
    }

    midiin.openPort(0);                      // open first port
    midiin.ignoreTypes(false, false, false); // receive all messages

    // set callback — push events into queue from RtMidi thread
    midiin.setCallback(&MIDIHandler::rtCallback, this);

    std::cout << "Opened MIDI port: " << midiin.getPortName(0) << "\n";
}

// existing poll() left intact for compatibility
bool MIDIHandler::poll(unsigned char& status,
                       unsigned char& note,
                       unsigned char& velocity)
{
    std::vector<unsigned char> msg;
    midiin.getMessage(&msg);
    if (msg.size() < 3)
        return false;

    status   = msg[0];
    note     = msg[1];
    velocity = msg[2];

    return true;
}

// RtMidi callback (static) — convert to MidiEvent and push
void MIDIHandler::rtCallback(double /*timeStamp*/, std::vector<unsigned char> *message, void *userData)
{
    if (!message || message->size() < 3) return;
    auto* self = static_cast<MIDIHandler*>(userData);
    if (!self) return;

    unsigned char status = (*message)[0];
    unsigned char note   = (*message)[1];
    unsigned char vel    = (*message)[2];

    MidiEvent e;
    if ((status & 0xF0) == 0x90 && vel > 0) {
        e.type = MidiEventType::NOTE_ON;
        e.note = note;
        e.vel  = vel;
        self->enqueueEvent(e);
    } else if ((status & 0xF0) == 0x80 || ((status & 0xF0) == 0x90 && vel == 0)) {
        e.type = MidiEventType::NOTE_OFF;
        e.note = note;
        e.vel  = vel;
        self->enqueueEvent(e);
    } else {
        // for now ignore other messages, or extend to CC later
    }
}

void MIDIHandler::enqueueEvent(const MidiEvent& e)
{
    if (!queue) return;
    if (!queue->push(e)) {
        // Queue full — best effort: drop silently or add a lightweight counter
        // Avoid printing here (could be called on RtMidi thread fast)
        // You can increment an atomic counter here to observe dropped counts.
    }
}

// event handlers unchanged (they mutate synth; now called from audio thread)
void MIDIHandler::onNoteOn(int channel, int note, int velocity)
{
    (void)channel;
    float velNorm = velocity / 127.0f;
    if (synth) synth->noteOn(note, velNorm);
}

void MIDIHandler::onNoteOff(int channel, int note)
{
    (void)channel;
    if (synth) synth->noteOff(note);
}

void MIDIHandler::onControlChange(int cc, int value)
{
    if (cc == 1) { // Mod Wheel
        float norm = value / 127.0f;
        for (auto& v : synth->voices)
            v.modWheelMod.value = norm;
    }
}

void MIDIHandler::onAftertouch(int pressure)
{
    float p = pressure / 127.0f;
    for (auto& v : synth->voices)
        v.aftertouchMod.value = p;
}

void MIDIHandler::onPitchBend(int value14bit)
{
    float norm = ((float)value14bit - 8192.0f) / 8192.0f;
    for (auto& v : synth->voices)
        v.pitchBendMod.value = norm;
}
