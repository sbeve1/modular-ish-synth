#include "input/ComputerKeyboard.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <cstdio>

#include "engine/Presets.h"

// semitone offset from C for each key, -1 if unmapped
static int keyToSemitone(unsigned short vk)
{
    switch (vk) {
        case 'A': return 0;  case 'W': return 1;  case 'S': return 2;
        case 'E': return 3;  case 'D': return 4;  case 'F': return 5;
        case 'T': return 6;  case 'G': return 7;  case 'Y': return 8;
        case 'H': return 9;  case 'U': return 10; case 'J': return 11;
        case 'K': return 12; case 'O': return 13; case 'L': return 14;
        default:  return -1;
    }
}

bool ComputerKeyboard::open()
{
    std::fill(std::begin(heldNote), std::end(heldNote), -1);
    std::fill(std::begin(isDown), std::end(isDown), false);

    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;
    if (h == INVALID_HANDLE_VALUE || !GetConsoleMode(h, &mode))
        return false;

    input = h;
    return true;
}

bool ComputerKeyboard::poll(unsigned timeoutMs)
{
    HANDLE h = (HANDLE)input;
    if (WaitForSingleObject(h, timeoutMs) != WAIT_OBJECT_0)
        return true;

    INPUT_RECORD records[32];
    DWORD count = 0;
    if (!ReadConsoleInputW(h, records, 32, &count))
        return true;

    for (DWORD i = 0; i < count; ++i) {
        if (records[i].EventType != KEY_EVENT)
            continue;

        const KEY_EVENT_RECORD& k = records[i].Event.KeyEvent;
        if (k.bKeyDown && k.wVirtualKeyCode == VK_RETURN)
            return false;

        if (k.bKeyDown) keyDown(k.wVirtualKeyCode);
        else            keyUp(k.wVirtualKeyCode);
    }
    return true;
}

void ComputerKeyboard::keyDown(unsigned short vk)
{
    if (vk >= 256 || isDown[vk])
        return; // auto-repeat
    isDown[vk] = true;

    if (vk == 'Z' || vk == 'X') {
        octaveShift = std::clamp(octaveShift + (vk == 'X' ? 1 : -1), -3, 3);
        std::printf("Octave: C%d\n", 4 + octaveShift);
        return;
    }

    if (vk >= '1' && vk < '1' + NUM_PRESETS) {
        int preset = vk - '1';
        queue.push({ MidiEventType::PROGRAM_CHANGE, (uint8_t)preset, 0 });
        std::printf("Preset %d: %s\n", preset + 1, PRESETS[preset].name);
        return;
    }

    int semi = keyToSemitone(vk);
    if (semi < 0)
        return;

    int note = std::clamp(60 + octaveShift * 12 + semi, 0, 127);
    if (queue.push({ MidiEventType::NOTE_ON, (uint8_t)note, 100 }))
        heldNote[vk] = note;
}

void ComputerKeyboard::keyUp(unsigned short vk)
{
    if (vk >= 256)
        return;
    isDown[vk] = false;

    if (heldNote[vk] < 0)
        return;

    // release the note that was pressed, even if the octave changed since
    queue.push({ MidiEventType::NOTE_OFF, (uint8_t)heldNote[vk], 0 });
    heldNote[vk] = -1;
}

void ComputerKeyboard::printHelp()
{
    std::printf(
        "Computer keyboard:\n"
        "  A W S E D F T G Y H U J K O L  play notes (from C4)\n"
        "  Z / X                          octave down / up\n");
    for (int i = 0; i < NUM_PRESETS; ++i)
        std::printf("  %d                              %s%s\n",
                    i + 1, PRESETS[i].name, i == 0 ? " (default)" : "");
    std::printf("  Enter                          quit\n");
}
