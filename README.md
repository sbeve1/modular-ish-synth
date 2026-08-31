# Real-Time Software Synthesizer (C++)

A real-time polyphonic software synthesizer written in modern C++, built as a portfolio / university DSP project. It focuses on DSP fundamentals, real-time engine architecture, and audio-thread constraints rather than UI polish or product completeness. The goal is to demonstrate a correctly engineered signal path from MIDI input to audio output, not to build a feature-complete commercial instrument.

Author: [Tevin Nguyen] - [tevinsynguyen@gmail.com]

---

## Features

- Polyphonic voice engine
- Wavetable oscillator with unison, detune, and stereo spread
- ADSR envelopes and LFOs
- Sample-accurate modulation matrix
- Dual multimode filters (LP / BP / HP)
- Flexible routing system (main, direct, bus sends)
- Send-style FX architecture
- Plate-style reverb
- Delay
- Real-time MIDI input (RtMidi)
- Low-latency audio output (miniaudio)

---

## High-Level Architecture

### Engine
- Voice allocation and mixing
- Global FX buses
- Master output stage and limiting

### Voice
- Oscillators → filters → routing → output
- All DSP state is owned per voice
- No shared mutable audio state between voices

### Control
- ADSR envelopes and LFOs
- Performance modulation sources
- Central modulation matrix
- Modulation evaluated at sample rate

### FX
- Send/return FX buses
- FX units output **wet-only** signal
- Dry/wet mixing handled at the engine level
- Bus-based FX design to avoid feedback issues

---

## Build Instructions

### Linux / macOS
```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Windows (Visual Studio)
```bash
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

---

## Running the Synth

1. Connect a MIDI keyboard.
2. Launch the executable:
   - `Synth` (Linux / macOS)
   - `Synth.exe` (Windows)
3. Play notes in real time.
4. Press **ENTER** in the console to quit.

---

## Design Notes

- No heap allocation in the audio callback.
- All DSP state is owned per voice.
- Sample-accurate modulation throughout the engine.
- Send-based FX architecture for stability and clarity.
- Conservative gain staging to preserve headroom.
- Architecture prioritized for correctness and readability.

---

## Known Limitations

- No graphical user interface.
- Reverb and FX are intentionally simple.
- Limited preset and patch management.
- Not optimized for large-scale commercial deployment.

---

## Future Work

- Graphical UI (JUCE or Dear ImGui).
- Additional FX (chorus, distortion, saturation).
- Preset system and patch browser.
- Oversampling for filters.

---

## License

This project is licensed under the MIT License. See the `LICENSE` file for details.
