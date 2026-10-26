# DSP ENGINE

Highly optimized **64-bit** audio processing engine for Flutter music players and native apps.

**Version 0.3.2** — cross-platform with AAudio (Android), WASAPI (Windows) and Core Audio (macOS/iOS).

## Features

- **Direct Volume Control (DVC)** — clean gain without digital clipping artefacts
- **Multi-band parametric EQ** (peak, shelves, LP/HP)
- **True-peak limiter** with fast attack / configurable release
- **Dither** (TPDF / noise-shaped)
- **Sample-rate conversion** (linear, cubic, HQ windowed-sinc)
- **Lock-free SPSC ring buffer**
- **Double-precision internal path** (optional)
- **Real-time thread priority** helpers
- **Platform audio backends**:
  - Android → AAudio (low-latency / exclusive)
  - Windows → WASAPI (shared + exclusive)
  - macOS / iOS → Core Audio (RemoteIO / Default Output)
- **C ABI** + Flutter `dart:ffi` bindings
- **Unit tests + benchmarks** + GitHub Actions CI matrix

## Quick start (CMake)

```bash
cmake -B build -DDSP_BUILD_TESTS=ON -DDSP_BUILD_EXAMPLES=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/tests/benchmarks
```

Backends are **auto-enabled** on the matching host OS. For cross-compiles pass the options explicitly:

```bash
-DDSP_BACKEND_AAUDIO=ON      # Android
-DDSP_BACKEND_WASAPI=ON      # Windows
-DDSP_BACKEND_COREAUDIO=ON   # Apple
```

## Flutter

See `bindings/flutter/dsp_engine/`. The Android `build.gradle` already enables the AAudio backend.

```dart
final engine = DspEngine.create(sampleRate: 48000, channels: 2);
engine.setEqBands([...]);
engine.process(input, output);
```

## Public C API (excerpt)

```c
DspEngine* dsp_create(const DspConfig* cfg);
DspError   dsp_process(DspEngine* eng, const float* in, float* out, int frames);
DspError   dsp_process_src(...);
DspError   dsp_eq_set_bands(...);
DspError   dsp_dvc_set_gain_db(...);
DspError   dsp_limiter_enable(...);
DspError   dsp_dither_set_mode(...);

// Optional platform output
DspBackend* dsp_backend_create(void);
DspError    dsp_backend_open(...);
DspError    dsp_backend_start(..., DspRenderCallback cb, void* user);
```

## Roadmap (remaining)

- ALSA / PipeWire backends for Linux
- More sophisticated HQ SRC (polyphase)
- SIMD-accelerated EQ & limiter paths
- Flutter example app polish

## License

MIT — see [LICENSE](LICENSE).
