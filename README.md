# Nullwave

A polyphonic wavetable synthesizer for Windows, written in C++. Nullwave has a
real-time audio engine, a GUI built with Dear ImGui, MIDI keyboard support,
and a headless tool for rendering, benchmarking and self-testing.

![Nullwave](docs/screenshot.png)

## Features

**Sound**
- 2 wavetable oscillators with 8 morphable frames each, band-limited with FFT
  mipmaps so high notes don't alias
- 9 factory wavetable banks, plus a wavetable editor: draw frames with the mouse,
  generate shapes or harmonics, morph-fill between frames
- Unison up to 16 voices per oscillator (detune, blend, stereo width), 5 warp
  modes (bend, PWM, sync, mirror, fold), a sub oscillator, and noise (white, pink, brown)
- Multimode zero-delay-feedback filter: LP/BP/HP at 12 and 24 dB, notch, peak,
  drive, key tracking
- 3 envelopes with curve control and 4 LFOs (7 shapes, free/retrigger/one-shot,
  tempo sync, rise, smoothing)
- Mod matrix with 32 slots: any source to any parameter, with aux scaling,
  Add/Multiply modes and response curves
- Effects: distortion (5 modes), chorus, ping-pong delay, 8-line FDN reverb,
  output limiter
- Up to 16 voices, poly/mono/legato, glide, sustain pedal
- 16 factory presets; user presets are saved as readable text files

**Playing**
- MIDI keyboards: notes, velocity, pitch bend, mod wheel, sustain, aftertouch,
  program change, and MIDI learn on any knob
- Computer keyboard: `A W S E D F T G Y H U J K O L P` play notes, `Z`/`X`
  change octave, `C`/`V` change velocity
- On-screen piano (velocity depends on where you click the key), plus pitch and
  mod wheels

**Interface**
- 3D wavetable view (drag to rotate) that follows the playing position
- Knobs show modulation rings and the live modulated value. Drag a mod source
  onto any knob to route it; Alt+drag sets the depth
- Envelope editor with draggable points and curves, LFO view with a live phase
  marker, draggable filter response curve
- Oscilloscope, spectrum analyzer, level meters and a CPU graph

## Build (Windows)

Requirements: Visual Studio 2022 with the C++ workload, and CMake 3.15 or newer.
GLFW and Dear ImGui are downloaded automatically when CMake configures.

```bash
cmake -S . -B build/vs -G "Visual Studio 17 2022" -A x64
```

```bash
cmake --build build/vs --config Release
```

This produces:

| Executable | Path | Purpose |
|---|---|---|
| `Nullwave.exe` | `build/vs/app/Release/` | the synthesizer |
| `nullwave_cli.exe` | `build/vs/app/Release/` | render, benchmark, self-test |
| `Synth.exe` | `build/vs/Release/` | the original console engine |

## Using the CLI

```bash
build/vs/app/Release/nullwave_cli.exe selftest
```

```bash
build/vs/app/Release/nullwave_cli.exe render "Aurora Pad" pad.wav
```

```bash
build/vs/app/Release/nullwave_cli.exe bench
```

`selftest` checks the following:
- every factory preset renders finite, in-range audio and releases all of its voices
- the audio path makes zero heap allocations (counted by overriding `operator new`)
- parameters round-trip between plain and normalized values
- the mod matrix changes the sound
- preset files round-trip
- the lock-free event queue works under a two-thread stress test

## Architecture

```
 UI thread (Dear ImGui, vsync)       MIDI thread (RtMidi)
   knobs ─────► ParamStore ◄──────── MIDI learn (CC -> param)
                (atomic floats)
   keys/piano ► SPSC ring            notes/CC ────► SPSC ring
   wavetables ► atomic pointer swap (freed after 2 audio callbacks)
                        │
                        ▼
              Audio thread (miniaudio callback)
              snapshot params + mod slots (once per block)
              voices: osc A/B + sub + noise ─► filter ─► amp
              mod matrix every 16 samples
              distortion ─► chorus ─► delay ─► reverb ─► master ─► limiter
                        │
                        ▼
              Telemetry (atomics): scope, meters, newest voice's
              modulation state ─► read by the UI for visuals
```

- The threads never lock. Parameters are atomics that the audio thread reads
  once per block. Notes go through single-producer/single-consumer rings, one
  ring per producer thread.
- The audio callback allocates nothing; all buffers are created in `prepare()`.
- `app/engine` has no GUI or device code, so the same engine can drive the CLI
  and, later, a WebAssembly/AudioWorklet build.

Measured on this machine (48 kHz), with a heavy patch: 2 oscillators × 8
unison, sub, noise, a 24 dB filter and all effects on.

| Voices | Speed (× realtime) | CPU at realtime |
|---|---|---|
| 1 | 95× | ~1% |
| 8 | 25× | ~4% |
| 16 | 13× | ~8% |

## Project layout

```
app/engine/   instrument, voices, wavetables, filter, LFOs, FX, presets
app/host/     audio device (miniaudio) and MIDI input (RtMidi)
app/ui/       theme, widgets, visualizations, pages, wavetable editor
app/tools/    nullwave_cli
src/          the original engine (console synth); Nullwave reuses its
              envelope, filter math and SPSC ring design
docs/         DEVLOG.md (development log)
```

## License

MIT. See `LICENSE.txt`.
