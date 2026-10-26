import 'dart:ffi';
import 'dart:typed_data';
import 'package:ffi/ffi.dart';

import 'dsp_engine_bindings.dart';
import 'dsp_config.dart';
import 'dsp_exception.dart';

/// High-level Dart wrapper around the native DSP ENGINE.
class DspEngine {
  final Pointer<Void> _handle;
  final DspEngineBindings _b;

  DspEngine._(this._handle, this._b);

  /// Create a new engine instance.
  static DspEngine create({
    double sampleRate = 48000,
    int channels = 2,
    int maxBlock = 1024,
    bool enableEq = true,
    bool enableDvc = true,
    bool enableLimiter = false,
    bool enableDither = false,
    DitherMode ditherMode = DitherMode.tpdf,
    double limiterThresholdDb = -1.0,
    double limiterReleaseMs = 50.0,
  }) {
    final b = DspEngineBindings();
    final cfg = calloc<DspConfigNative>();
    cfg.ref.sampleRate = sampleRate;
    cfg.ref.channels = channels;
    cfg.ref.maxBlock = maxBlock;
    cfg.ref.enableEq = enableEq ? 1 : 0;
    cfg.ref.enableDvc = enableDvc ? 1 : 0;
    cfg.ref.enableLimiter = enableLimiter ? 1 : 0;
    cfg.ref.enableDither = enableDither ? 1 : 0;
    cfg.ref.ditherMode = ditherMode.index;
    cfg.ref.limiterThresholdDb = limiterThresholdDb;
    cfg.ref.limiterReleaseMs = limiterReleaseMs;

    final h = b.dsp_create(cfg);
    calloc.free(cfg);
    if (h == nullptr) {
      throw DspException('dsp_create failed');
    }
    return DspEngine._(h, b);
  }

  static String get version {
    final b = DspEngineBindings();
    final p = b.dsp_version();
    return p.toDartString();
  }

  void dispose() {
    _b.dsp_destroy(_handle);
  }

  void reset() => _check(_b.dsp_reset(_handle));

  void setSampleRate(double rate) =>
      _check(_b.dsp_set_sample_rate(_handle, rate));

  void setChannels(int ch) => _check(_b.dsp_set_channels(_handle, ch));

  /// Process interleaved float32 samples.
  void process(Float32List input, Float32List output) {
    if (input.length != output.length) {
      throw ArgumentError('input/output length mismatch');
    }
    final frames = input.length ~/ 2; // assume stereo for simplicity; real code uses channels
    final inPtr = calloc<Float>(input.length);
    final outPtr = calloc<Float>(output.length);
    for (var i = 0; i < input.length; i++) inPtr[i] = input[i];
    _check(_b.dsp_process(_handle, inPtr, outPtr, frames));
    for (var i = 0; i < output.length; i++) output[i] = outPtr[i];
    calloc.free(inPtr);
    calloc.free(outPtr);
  }

  void setEqEnabled(bool enable) =>
      _check(_b.dsp_eq_enable(_handle, enable ? 1 : 0));

  void setEqBands(List<EqBand> bands) {
    if (bands.isEmpty) {
      _check(_b.dsp_eq_set_bands(_handle, nullptr, 0));
      return;
    }
    final ptr = calloc<DspEqBandNative>(bands.length);
    for (var i = 0; i < bands.length; i++) {
      ptr[i].freqHz = bands[i].freqHz;
      ptr[i].gainDb = bands[i].gainDb;
      ptr[i].q = bands[i].q;
      ptr[i].type = bands[i].type.index;
    }
    _check(_b.dsp_eq_set_bands(_handle, ptr, bands.length));
    calloc.free(ptr);
  }

  void setDvcGainDb(double db) =>
      _check(_b.dsp_dvc_set_gain_db(_handle, db));

  double get dvcGainDb => _b.dsp_dvc_get_gain_db(_handle);

  void setLimiterEnabled(bool enable) =>
      _check(_b.dsp_limiter_enable(_handle, enable ? 1 : 0));

  void setLimiterThreshold(double db) =>
      _check(_b.dsp_limiter_set_threshold(_handle, db));

  void setLimiterRelease(double ms) =>
      _check(_b.dsp_limiter_set_release(_handle, ms));

  void setDitherEnabled(bool enable) =>
      _check(_b.dsp_dither_enable(_handle, enable ? 1 : 0));

  void setDitherMode(DitherMode mode) =>
      _check(_b.dsp_dither_set_mode(_handle, mode.index));

  void _check(int code) {
    if (code != 0) throw DspException('DSP error $code');
  }
}

enum DitherMode { none, tpdf, shaped }

enum EqType { peak, lowShelf, highShelf, lowPass, highPass }

class EqBand {
  final double freqHz;
  final double gainDb;
  final double q;
  final EqType type;
  const EqBand({
    required this.freqHz,
    required this.gainDb,
    this.q = 1.0,
    this.type = EqType.peak,
  });
}
