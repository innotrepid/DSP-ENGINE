# Changelog

## 0.3.2 — WASAPI & Core Audio backends
- **WASAPI** (Windows): exclusive mode with shared fallback, event-driven render thread, MMCSS "Pro Audio", float32 preferred
- **Core Audio** (macOS Default Output / iOS RemoteIO): AU render callback, interleaved or non-interleaved float
- `create_default_backend()` selects AAudio / WASAPI / Core Audio / Null automatically
- CMake: `-DDSP_BACKEND_WASAPI=ON` (links ole32, avrt), `-DDSP_BACKEND_COREAUDIO=ON` (AudioUnit, AudioToolbox, CoreAudio)
- CI: Windows builds with WASAPI, macOS builds with Core Audio

## 0.3.1 — AAudio backend
- Full Android **AAudio** output backend (`backend_aaudio.cpp`)
  - LOW_LATENCY performance mode, exclusive sharing with shared fallback
  - Float32 interleaved, media/music usage attributes
  - Data + error callbacks, xrun counting, negotiated rate/channels/burst
- C ABI: `dsp_backend_create/open/start/stop/...`, `dsp_backend_set_render_callback`, `dsp_backend_set_engine`
- CMake: `-DDSP_BACKEND_AAUDIO=ON` links `libaaudio` + `liblog` on Android
- Flutter Android plugin enables AAudio by default (minSdk 26)
- CI Android builds compile with AAudio enabled
- Unit test for backend lifecycle (null backend on host)

## 0.3.0 — Remaining core features
- **True-peak limiter API**: enable/threshold, true-peak metering (`dsp_limiter_*`)
- **Dither**: TPDF and 1st-order noise-shaped modes for 16/24-bit style quantization
- **Sample-rate conversion API**: `dsp_process_src` with linear / cubic / HQ quality
- Improved internal resampler (windowed-sinc HQ path)
- Audio backend abstraction (`IAudioBackend` + NullBackend); platform backends still optional
- Flutter FFI package updated for limiter, dither, and SRC
- Unit tests expanded (26 tests)
- Version 0.3.0

## 0.2.3 — CI with tests & benchmarks
- Expanded GitHub Actions CI:
  - Linux (Release/Debug + double/float precision matrix): build, unit tests, example, benchmarks
  - macOS & Windows: build, unit tests, example, benchmarks
  - Android NDK (arm64-v8a, armeabi-v7a) shared library artifacts
  - Benchmark results uploaded as artifacts
  - clang-tidy static analysis (non-blocking)
- Unit tests and benchmarks run automatically on every push/PR

## 0.2.2 — Tests & benchmarks
- Added self-contained unit test suite (`tests/unit_tests.cpp`) covering:
  - Lifecycle, volume/DVC, ramping, multi-channel process, planar API
  - 31-band EQ set/get/reset/bulk, enable/disable
  - Soft limiter behaviour, null-safety, edge cases
- Added performance benchmarks (`tests/benchmarks.cpp`) measuring MSamples/s and realtime headroom
- CMake: `DSP_BUILD_TESTS=ON` builds tests + registers them with CTest
- Custom targets: `run_tests`, `run_benchmarks`

## 0.2.1 — Stability & missing pieces
- Fixed CMake: static-only and shared builds both work cleanly (no more ALIAS target errors)
- Hardened process path:
  - Full 1–8 channel support with planar temp buffers
  - No heap allocations on the normal process path
  - Safer volume ramp (atomic remaining count)
  - Soft peak limiter after DVC (prevents hard clips after EQ/volume boosts)
  - Graceful bypass + xrun count on buffer-grow failure
- Implemented usable cubic / linear sample-rate converter (internal, ready for public API)
- Expanded DVC helpers (true-peak estimate, TPDF dither)
- Proper header for lock-free SPSC ring buffer
- Improved offline example (EQ + ramped volume)
- Version string now `0.2.1`

## 0.2.0 — Multi-band EQ
- Native 31-band parametric EQ (RBJ peaking, double-precision)
- Lock-free double-buffered coefficient updates
- C ABI: `dsp_eq_set_enabled`, `dsp_eq_set_band`, `dsp_eq_set_bands`, `dsp_eq_reset`
- Flutter: `eqEnabled`, `setEqBand`, `setEqBands`, `resetEq`
- Process path: EQ → DVC
- Version string: `0.2.0-eq31`

## 0.1.0 — Initial
- 64-bit processing path, Direct Volume Control, Flutter FFI, Android NDK
