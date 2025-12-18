#include "MIDIHandler.h"
#include <iostream>

MIDIHandler::MIDIHandler() {
    unsigned int nPorts = midiin.getPortCount();
    
    if (nPorts == 0) {
        std::cout << "No MIDI devices found.\n";
        return;
    }

    midiin.openPort(0);
    midiin.ignoreTypes(false, false, false);
    std::cout << "Opened MIDI device.\n";
}

bool MIDIHandler::poll(unsigned char& status, unsigned char& note, unsigned char& velocity) {
    std::vector<unsigned char> msg;
    double stamp = midiin.getMessage(&msg);

    if (msg.size() < 3) return false;

    status = msg[0];
    note = msg[1];
    velocity = msg[2];
    return true;
}
