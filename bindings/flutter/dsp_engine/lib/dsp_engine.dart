/// Flutter FFI bindings for [DSP ENGINE](https://github.com/innotrepid/DSP-ENGINE).
///
/// High-level Dart API over the C ABI (`dsp_engine.h`).
///
/// **App identity:** This is a library plugin. When used inside Resonate it
/// inherits Resonate's application id (`com.aetherion.resonate` /
/// `com.Aetherion.Resonate`). It does **not** use a separate
/// `com.aetherion.resonatedsp` app id.
library dsp_engine;

export 'src/dsp_engine.dart';
export 'src/dsp_config.dart';
export 'src/dsp_exception.dart';
