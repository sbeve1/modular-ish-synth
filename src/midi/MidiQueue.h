// src/midi/MidiQueue.h
#pragma once
#include <atomic>
#include <cstdint>
#include <cstring>

enum class MidiEventType { NOTE_ON, NOTE_OFF, PROGRAM_CHANGE }; // PROGRAM_CHANGE: note = preset index

struct MidiEvent {
    MidiEventType type;
    uint8_t note;
    uint8_t vel;   // 0..127
};

// Power-of-two ring buffer single-producer single-consumer
template<int N>
class MidiRing {
    static_assert((N & (N - 1)) == 0, "N must be power of two");
public:
    MidiRing() : head(0), tail(0) {}
    bool push(const MidiEvent& e) {
        int h = head.load(std::memory_order_relaxed);
        int next = (h + 1) & (N - 1);
        if (next == tail.load(std::memory_order_acquire)) return false; // full
        buffer[h] = e;
        head.store(next, std::memory_order_release);
        return true;
    }
    bool pop(MidiEvent& out) {
        int t = tail.load(std::memory_order_relaxed);
        if (t == head.load(std::memory_order_acquire)) return false; // empty
        out = buffer[t];
        tail.store((t + 1) & (N - 1), std::memory_order_release);
        return true;
    }
private:
    MidiEvent buffer[N];
    std::atomic<int> head, tail;
};
