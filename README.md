# DSP ENGINE

[![CI](https://github.com/innotrepid/DSP-ENGINE/actions/workflows/ci.yml/badge.svg)](https://github.com/innotrepid/DSP-ENGINE/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![CMake](https://img.shields.io/badge/CMake-3.16%2B-green.svg)](https://cmake.org/)

**Highly optimized 64-bit audio processing engine** featuring unique **Direct Volume Control (DVC)** for ultra-low latency, bit-perfect, high-resolution output while maintaining near-real-time priority and preventing audio drops.

Designed from the ground up to be integrated into music players (for example the Flutter-based [Resonate](https://github.com/R-6167/Resonate)) via a clean C ABI / FFI.

---

## Features

| Feature | Description |
|---------|-------------|
| **64-bit processing** | Internal path uses `float64` (double) for maximum precision |
| **Direct Volume Control (DVC)** | Gain is applied in the high-precision domain before final conversion |
| **Smooth ramping** | Click-free / zipper-free volume changes |
| **Bit-perfect path** | Unity-gain pass-through when no processing is active |
| **Lock-free buffers** | SPSC ring buffer foundation for drop-free operation |
| **Real-time priority** | Helpers for Windows, Linux, macOS and Android |
| **Clean C ABI** | Ready for Flutter `dart:ffi`, Rust, Python, or any language that can call C |
| **Cross-platform CMake** | Shared & static libraries, install rules, `find_package` support |

---

## Quick start

```bash
git clone https://github.com/innotrepid/DSP-ENGINE.git
cd DSP-ENGINE
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
./examples/dsp_example_offline
```

### Useful CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `DSP_BUILD_SHARED` | `ON` | Build shared library (recommended for FFI) |
| `DSP_BUILD_STATIC` | `OFF` | Build static library |
| `DSP_BUILD_EXAMPLES` | `ON` | Build example programs |
| `DSP_DOUBLE_PRECISION` | `ON` | Use float64 for internal processing |
| `DSP_BIT_PERFECT` | `ON` | Enable pure pass-through when possible |
| `DSP_ENABLE_SIMD` | `ON` | AVX2 / NEON optimizations |
| `DSP_ENABLE_RT` | `ON` | Real-time priority support |
| `DSP_BACKEND_AAUDIO` | `OFF` | Android AAudio backend |
| `DSP_BACKEND_WASAPI` | `OFF` | Windows WASAPI exclusive |
| `DSP_BACKEND_COREAUDIO` | `OFF` | macOS / iOS Core Audio |
| `DSP_BACKEND_ALSA` | `OFF` | Linux ALSA |
| `DSP_BACKEND_JACK` | `OFF` | Linux JACK |
| `DSP_BACKEND_PIPEWIRE` | `OFF` | Linux PipeWire |

---

## Public API (C)

See [`include/dsp_engine.h`](include/dsp_engine.h) for the complete ABI.

Minimal example:

```c
#include "dsp_engine.h"

int main(void) {
    DspConfig cfg = dsp_config_default();
    DspEngine* eng = dsp_create(&cfg);
    if (!eng) return 1;

    dsp_set_volume_db(eng, -6.0);          /* Direct Volume Control */
    dsp_process(eng, in, out, frames);     /* float32 interleaved   */

    dsp_destroy(eng);
    return 0;
}
```

---

## Integration with Flutter / Resonate

1. Build the shared library for your target platform(s).
2. Use `dart:ffi` (or a thin plugin) to call the C API.
3. Route decoded PCM from your player through `dsp_process()` before it reaches the device.

A ready-to-use Flutter FFI package will live under `bindings/flutter/` in a future release.

---

## Project structure

```
DSP-ENGINE/
├── include/dsp_engine.h          # Public C ABI
├── src/
│   ├── core/                     # Processor, DVC, resampler
│   ├── audio/                    # Ring buffer + future backends
│   ├── realtime/                 # Thread priority helpers
│   └── utils/
├── examples/                     # Offline processing demo
├── cmake/                        # Package config
├── .github/workflows/ci.yml      # Multi-platform CI
└── CMakeLists.txt
```

---

## Roadmap

- [ ] High-quality sample-rate converter
- [ ] Parametric / graphic EQ in the 64-bit domain
- [ ] True-peak limiter + noise-shaped dither
- [ ] Platform audio backends (AAudio exclusive, WASAPI exclusive, Core Audio, PipeWire)
- [ ] Flutter / Dart FFI package
- [ ] Benchmarks & latency measurements
- [ ] Unit test suite

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).  
Please also read our [Code of Conduct](CODE_OF_CONDUCT.md).

## License

This project is licensed under the **MIT License** – see the [LICENSE](LICENSE) file for details.

## Acknowledgments

Built to power high-resolution, low-latency playback in modern music players.
