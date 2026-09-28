# Changelog

All notable changes to DSP ENGINE will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2026-09-28

### Added
- Initial public C ABI (`dsp_engine.h`)
- 64-bit (float64) internal processing path
- Direct Volume Control (DVC) with linear and dB setters
- Smooth volume ramping (click-free)
- Bit-perfect unity-gain pass-through
- Lock-free SPSC ring buffer foundation
- Real-time / near-real-time thread priority helpers (Windows, Linux, macOS, Android)
- CMake build system with shared/static library support
- Offline processing example
- GitHub Actions CI (Ubuntu, Windows, macOS)
- Installation & `find_package` support

### Notes
- Platform audio backends (AAudio, WASAPI, Core Audio, etc.) are stubbed and ready for implementation.
- High-quality resampler, EQ, and limiter are planned for upcoming releases.
