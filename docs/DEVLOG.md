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

## Phase 2: GUI instrument "modular_synth" (branch `feature/gui-synth`, worktree `CRAZY PROJECT-gui`)

Goal: turn the engine into a complete, playable synthesizer with a modern
(Serum-2-inspired, original design) interface. Desktop first; browser build
(WebAssembly + AudioWorklet + Web MIDI) planned from the same codebase.

### Architecture decisions

- **Desktop app in C++ with Dear ImGui (GLFW + OpenGL 3).** Reuses the engine,
  miniaudio and RtMidi directly; native audio latency; UI rendering on its own
  thread, never in the audio callback.
- **Engine library (`modular_synth_engine`) has no GUI or device code**, so the same
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

### Measured
- Engine benchmark (48 kHz, 2 osc x 8 unison + sub + noise + 24 dB filter, all FX):
  16 voices render at 12.9x realtime (~8% of one core); 1 voice at 95x.
- Self-test: 0 heap allocations across 400 audio blocks with notes, FX and
  parameter changes; all 16 factory presets render finite, in range, and
  release every voice.
- GUI at idle with 5 voices sounding: ~2% engine load (WASAPI, 10 ms buffer).

### Bugs found and fixed while building
- RtMidi lives in `rt::midi` (forward declaration mismatch).
- Filled curves looked hatched: overlapping anti-aliased quads; fills now
  draw without AA.
- On-screen pitch wheel spring-back would have overridden the hardware wheel;
  it now springs back only when the on-screen wheel is released.

### Design iterations
- 3D wavetable: started as a rotated perspective view (frames looked flat),
  then an oblique projection with hidden-line fills (frames hid each other),
  then transparent line slices with only the playing slice filled.
- Visual identity: renamed Halcyon -> **modular_synth**; teal/violet palette
  replaced with neutral near-black, white text, one green accent for values
  and white for modulation.

### Round 2: instrument features
- **Arpeggiator** on the audio thread, stepped per control block (16 samples,
  0.33 ms) so timing doesn't depend on the UI. Physical keys and arp notes take
  separate voice paths, so mono/legato fallback and the sustain pedal don't
  interfere with arp notes. Test: Up mode at 1/16, 120 BPM plays 60-64-67 in
  order at the right rate.
- **Second filter** with series/parallel routing (test: LP 400 Hz into HP 4 kHz
  cuts to silence in series, passes in parallel).
- **Drawable LFO shapes**: breakpoints with per-segment curves, rendered to a
  256-point table on the UI thread and published with the same epoch-based
  pointer swap as wavetables (the manager became `ResourceManager`).
- **Effects**: 3-band EQ (RBJ biquads), compressor (stereo-linked, soft knee,
  gain reduction shown in the UI), 6-stage phaser, flanger. The rack order is a
  single atomic, one nibble per slot, so reordering is lock-free and saved in
  presets.
- **Undo/redo**: snapshot history that commits once changes settle (mouse up,
  nothing moving for 350 ms), so a knob drag, a MIDI CC sweep or a preset load
  is one step no matter what caused it.
- Zero-allocation test now runs with everything on: arp, both filters, custom
  LFO, all 8 effects in reversed order.

### Round 3: readability and identity
- Renamed to **modular_synth**.
- Accent switched to neon green (#00FF41); modulation stays white.
- Wavetable view rebuilt for readability, modeled on how Serum presents
  wavetables: only the 8 real frames (was 29 lines including interpolated
  slices), numbered, spaced apart and fading with distance from the playing
  position; the playing frame is filled and marked on a depth rail; a 2D
  strip below shows the exact output waveform with the warp mode applied
  (same phase-warp math as the oscillator).
- Iterated on the wavetable view with feedback, using Serum 2's own display
  as the reference:
  - removed frame numbers; added a grey 3D box around the stack (half
    opacity); accent moved to a lime-leaning neon green (#6EFF28)
  - taller waves; tried a screen-space rotation (tilted the floor, rejected)
    and a true 3D yaw (box too narrow for the wide display)
  - final: an oblique projection with proportions measured from a Serum 2
    screenshot (waveform axis right and slightly down, depth right and up, a
    wide low box), 15 lightly filled slices that read as a shaded terrain,
    playing frame bright and filled; the projection is fitted so the box
    always fills the view
  - removed the separate position dot and rail (the highlighted frame already
    shows the position)
- Automated UI checks: the app's `--screenshot` flag plus `--set key=value`,
  `--page`, `--fx` let every visual change be verified from a capture.

### Round 4: optimization pass (no features cut)
Method: my own review plus an independent read-only subagent audit (given
the rule "optimize, don't cut"); an extended benchmark (best of 3 runs) with
a per-part breakdown; and a per-preset RMS/peak fingerprint to prove the
sound didn't change (13 of 17 presets bit-identical, the rest within 0.07%,
all from control-rate chorus modulation).

| Scenario (48 kHz) | Before | After | Change |
|---|---|---|---|
| Heavy patch, 16 voices | 7.58% of a core (13.2x RT) | 6.11% (16.4x RT) | 19% faster |
| Max: 2x16 unison, bend+sync warp, 2 filters, all 8 FX | 30.17% | 22.60% | 25% faster |
| FX rack only (all 8 effects) | 1.42% | 0.96% | 32% faster |

What changed:
- Fast, accurate math for per-sample paths: `fastExp2` (split at the nearest
  integer, ~1.5e-6), `fastLog2` (~1e-6), `fastSin` (~3e-6).
- Bend warp: `pow` per sample per unison voice replaced by a 4096-point
  log2 table plus a branch-free `exp2`, with an exact path near phase 0.
  The breakdown showed my first "exact" version was slower than the table
  (warp alone cost 25% of a core); the hybrid brought it to ~10%.
- Oscillator inner loop templated on warp mode (no per-sample switch),
  unison voices as the outer loop; detune ratios and stereo gains cached and
  rebuilt only when their inputs change.
- Filter loop templated on mode/slope/drive.
- Phaser coefficients computed per 16 samples and ramped; chorus/flanger
  modulation per 16 samples, interpolated; compressor dB math via fast
  log2/exp2.
- Modulated parameter values converted once per block, not per lookup.
- Original `Envelope`: time scale computed once per tick and skipped when key
  tracking is off; curve exponents cached (about 5 pow per tick down to 1).
- Effects run in 64-sample sub-blocks so modulated FX parameters update
  smoothly regardless of the audio buffer size.

Bugs found by the audit and fixed:
- Out-of-bounds wavetable read when an oscillator's frequency reached the
  sample rate (single `p -= 1` wrap); now wraps by the integer part.
- No denormal protection: FTZ/DAZ set at the top of `process()`.
- Switching the delay on cleared 2 MB inside one callback; delay lines now
  reset in O(1) with a fill counter (unwritten history reads as silence).
- Torn mod-slot reads between UI and audio threads; each slot now uses a
  seqlock.
- UI: undo copied the whole state every frame (now only when a change
  counter moves); editors allocated per frame; the 3D view cache was keyed
  by address (stale after reuse), now by a per-build table id.

### Round 5: resume polish
- `showcase` CLI command renders a 29 s multi-preset demo (pad, lead, bass,
  arpeggiator, glass pad) offline, balanced per part and normalized to
  -1 dBFS, written as 16-bit WAV: `docs/demo.wav`, linked from the README.
- GitHub Actions CI (`.github/workflows/ci.yml`, windows-latest): configure
  with VS 2022, build Release, run `selftest` (fails the build on any
  failure), run `bench` for the log, upload the executables. Verified by
  running the same commands on a clean build directory locally. Not pushed.

### Round 6: latency and LFO feedback from playing
- **Input latency.** Measured the real output buffer: Windows shared mode on
  a Focusrite held 970 frames (22 ms), and computer keys also waited for the
  next vsync frame (up to ~16 ms). Keys are now handled in the GLFW key
  callback (the UI loop wakes on input instead of blocking on vsync); output
  queues 2 periods instead of 3; the Global page shows the real latency.
- **ASIO.** Added an ASIO driver option through RtAudio (static, ASIO API
  only; it ships the ASIO host interface). Renders straight into the driver's
  non-interleaved buffers. On the Focusrite: 64 samples at 48 kHz, 3.9 ms
  reported output latency, 5.6x lower than WASAPI shared. `--asio [buffer]`
  starts on ASIO; falls back to WASAPI if the driver can't open.
- **"Fast/sharp LFOs don't do anything."** Measured first: a new
  `lfotest` command renders every shape at 2/10/40 Hz into Osc A level and
  measures the level swing; all were 1.00 (full), so the engine was fine.
  The real problems were in the UI: drops defaulted to 20% depth (easy to
  miss), and the knob showed one sampled value per frame, which aliases with
  fast LFOs and looks frozen or random. Drops now default to 50%, and the
  audio thread records the min/max each modulated parameter swept between UI
  frames; knobs draw that range as a bright band plus the current-value dot.

### Progress
- [x] Worktree + feature branch (original project untouched)
- [x] Engine: params, wavetables, voices, mod matrix, FX
- [x] Audio/MIDI hosts (latency selector, exclusive mode)
- [x] GUI: synth, matrix, FX, global pages; 3D wavetables; editors
- [x] Presets, wavetable editor
- [x] Self-test, benchmark, README
- [x] Arpeggiator, second filter, drawable LFO, EQ/compressor/phaser/flanger, undo/redo
- [x] Rename to modular_synth; Serum 2-style wavetable view
- [x] Optimization pass: 19-32% faster, audit bugs fixed, sound unchanged
- [x] Demo clip and CI workflow
- [x] ASIO, lower input latency, LFO feedback fixes
- [ ] Browser build (needs the Emscripten toolchain installed)
