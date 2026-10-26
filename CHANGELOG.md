# Changelog

## [0.3.2] — 2026-10-26

### Added
- Full cross-platform audio backends:
  - **AAudio** (Android) — low-latency / exclusive mode support
  - **WASAPI** (Windows) — shared + exclusive, Pro Audio thread priority
  - **Core Audio** (macOS/iOS) — RemoteIO / Default Output
- CMake auto-detection of host platform backends
- Complete public C API for limiter, dither, SRC and backends
- Self-contained unit-test framework + 12+ tests
- Throughput benchmarks (MSamples/s + realtime factor)
- GitHub Actions CI matrix (Linux / macOS / Windows / Android NDK)
- Flutter FFI bindings updated for all new features
- True-peak style limiter with configurable threshold & release
- TPDF / shaped dither
- Linear / cubic / HQ sample-rate conversion path

### Changed
- Internal processing uses double precision by default (`DSP_USE_DOUBLE`)
- Real-time safety hardened (no allocations on the process path)
- Version string now reports `0.3.2`

### Fixed
- Multi-channel no-allocation process path
- Limiter attack/release timing
- Backend factory stubs when a platform define is absent

## [0.2.0] — earlier
- Multi-band EQ + Flutter FFI
- Initial DVC and ring-buffer work
