# Development log

A running record of what changed, why, and what was measured. Useful for
interviews and resume bullets: each entry notes the engineering problem,
not just the feature.

---

## Phase 1: cleanup of the original engine (branch `cleanup/sample-rate-and-build`)

| Commit | What | Why it matters |
|---|---|---|
| `fab902d` | Stopped tracking `build/` (23 artifacts incl. `Synth.exe`) | Repo hygiene; `.gitignore` already excluded it |
| `3cd990b` | Fixed the MSVC build (`NOMINMAX`, `_USE_MATH_DEFINES`) | The project had only ever been built with MinGW; `windows.h` `min`/`max` macros broke `std::min`, `M_PI` was undefined |
| `a80a906` | `Synth::prepare(sampleRate)`; removed hardcoded `44100` FX defaults | Delay/reverb times were ~9% wrong on 48 kHz devices. FX are now built after `ma_device_init`, before `ma_device_start`, so there is no race with the audio callback |
| `91e9678` | Removed dead debug lambdas from the voice hot path | — |
| `a8720af` | Headless `--render out.wav` mode | Offline, sample-accurate rendering through the real engine: demo clips, tests, benchmarks. Measured ~28x realtime |
| `f26da4a` | **Fixed a mod-matrix bug that doubled every modulated parameter** | The matrix wrote `base + modulation` into fields that the resolvers then added to `base` again: osc level ran +6 dB hot and filter cutoff at 2x with zero routes. Also: routing was reset on every note-on, so bus sends could never stick |
| `f6c4e05` | Computer-keyboard input, patch struct, 4 presets | Second SPSC queue for keyboard events, because the MIDI queue already has the RtMidi thread as its only producer; preset changes applied on the audio thread |

## Phase 2: GUI instrument "Halcyon" (branch `feature/gui-synth`, worktree `CRAZY PROJECT-gui`)

Goal: turn the engine into a complete, playable synthesizer with a modern
(Serum-2-inspired, original design) interface. Desktop first; browser build
(WebAssembly + AudioWorklet + Web MIDI) planned from the same codebase.

### Architecture decisions

- **Desktop app in C++ with Dear ImGui (GLFW + OpenGL 3).** Reuses the engine,
  miniaudio and RtMidi directly; native audio latency; UI rendering on its own
  thread, never in the audio callback.
- **Engine library (`halcyon_engine`) has no GUI or device code**, so the same
  code can be compiled to WebAssembly later and driven by a headless CLI for
  tests and benchmarks.
- **Lock-free UI ↔ audio bridge.**
  - Parameters: one atomic float per parameter, stored *normalized*; the audio
    thread snapshots them once per block.
  - Notes/controllers: SPSC rings (one per producer thread: UI, MIDI).
  - Wavetables: built on the UI thread, published to audio with an atomic
    pointer swap; old tables are freed only after the audio callback counter
    has advanced (epoch-based reclamation), so audio never touches freed memory.
  - Visuals: audio publishes scope samples, meters, and the live modulation
    state of the newest voice through atomics; the UI only reads.
- **Mod matrix in normalized knob space** (like Serum): any parameter can be a
  target; 32 persistent slots with an optional aux source. Runs at control
  rate (every 16 samples) instead of per sample.
- **Band-limited wavetables:** 8 frames x 2048 samples, FFT'd into 11 mip
  levels; the oscillator picks the level whose top harmonic is below Nyquist,
  so high notes don't alias.

### Reused from the original engine
`Envelope` (curved ADSR), the ZDF state-variable filter math, the SPSC ring
design, miniaudio, RtMidi, the WAV render approach.

### Progress
- [x] Worktree + feature branch (original project untouched)
- [ ] Engine: params, wavetables, voices, mod matrix, FX
- [ ] Audio/MIDI hosts
- [ ] GUI
- [ ] Presets, wavetable editor
- [ ] Tests, benchmark, docs
- [ ] Browser build
