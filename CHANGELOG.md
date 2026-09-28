# Changelog

## 0.2.0 — Multi-band EQ
- Native 31-band parametric EQ (RBJ peaking, double-precision)
- Lock-free double-buffered coefficient updates
- C ABI: `dsp_eq_set_enabled`, `dsp_eq_set_band`, `dsp_eq_set_bands`, `dsp_eq_reset`
- Flutter: `eqEnabled`, `setEqBand`, `setEqBands`, `resetEq`
- Process path: EQ → DVC
- Version string: `0.2.0-eq31`

## 0.1.0 — Initial
- 64-bit processing path, Direct Volume Control, Flutter FFI, Android NDK
