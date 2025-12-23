#pragma once

struct Voice; // forward declaration

struct VoiceModulation {
    static void registerSources(Voice& v);
    static void registerTargets(Voice& v);
};