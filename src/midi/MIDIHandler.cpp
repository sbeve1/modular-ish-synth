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

    std::cout << "Opened MIDI port: " << midiin.getPortName(0) << "\n";
}

// ------------------------ POLL FUNCTION ------------------------
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

// ------------------------ NOTE ON ------------------------
void MIDIHandler::onNoteOn(int channel, int note, int velocity)
{
    (void)channel;
    float velNorm = velocity / 127.0f;
    synth->noteOn(note, velNorm);
}

// ------------------------ NOTE OFF ------------------------
void MIDIHandler::onNoteOff(int channel, int note)
{
    (void)channel;
    synth->noteOff(note);
}

// ------------------------ CC ------------------------
void MIDIHandler::onControlChange(int cc, int value)
{
    if (cc == 1) { // Mod Wheel
        float norm = value / 127.0f;
        for (auto& v : synth->voices)
            v.modWheelMod.value = norm;
    }
}

// ------------------------ AFTERTOUCH ------------------------
void MIDIHandler::onAftertouch(int pressure)
{
    float p = pressure / 127.0f;
    for (auto& v : synth->voices)
        v.aftertouchMod.value = p;
}

// ------------------------ PITCH BEND ------------------------
void MIDIHandler::onPitchBend(int value14bit)
{
    float norm = ((float)value14bit - 8192.0f) / 8192.0f;
    for (auto& v : synth->voices)
        v.pitchBendMod.value = norm;
}
