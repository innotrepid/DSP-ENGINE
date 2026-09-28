import 'dart:ffi';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';

import 'dsp_config.dart';
import 'dsp_engine_bindings.dart' as native;
import 'dsp_exception.dart';

/// High-level handle to a DSP ENGINE instance (DVC + multi-band EQ).
class DspEngine {
  Pointer<Void> _handle;
  bool _disposed = false;

  DspEngine._(this._handle);

  factory DspEngine.create([DspConfig config = const DspConfig()]) {
    final cfg = calloc<native.DspConfigNative>();
    try {
      cfg.ref
        ..sample_rate = config.sampleRate
        ..channels = config.channels
        ..buffer_frames = config.bufferFrames
        ..exclusive_mode = config.exclusiveMode
        ..bit_perfect = config.bitPerfect
        ..realtime_priority = config.realtimePriority;
      final handle = native.DspEngineNative.dsp_create(cfg);
      if (handle == nullptr) {
        final errPtr = native.DspEngineNative.dsp_last_error();
        final err = errPtr == nullptr ? '' : errPtr.toDartString();
        throw DspException(err.isEmpty ? 'dsp_create failed' : err);
      }
      return DspEngine._(handle);
    } finally {
      calloc.free(cfg);
    }
  }

  void _ensureAlive() {
    if (_disposed || _handle == nullptr) {
      throw const DspException('DspEngine already disposed');
    }
  }

  static String get version {
    try {
      final p = native.DspEngineNative.dsp_version();
      if (p == nullptr) return 'unknown';
      return p.toDartString();
    } catch (_) {
      return 'unknown';
    }
  }

  int get sampleRate {
    _ensureAlive();
    return native.DspEngineNative.dsp_get_sample_rate(_handle);
  }

  int get channels {
    _ensureAlive();
    return native.DspEngineNative.dsp_get_channels(_handle);
  }

  int get bufferFrames {
    _ensureAlive();
    return native.DspEngineNative.dsp_get_buffer_frames(_handle);
  }

  bool get isRunning {
    _ensureAlive();
    return native.DspEngineNative.dsp_is_running(_handle);
  }

  int get processedFrames {
    _ensureAlive();
    return native.DspEngineNative.dsp_get_processed_frames(_handle);
  }

  int get xrunCount {
    _ensureAlive();
    return native.DspEngineNative.dsp_get_xrun_count(_handle);
  }

  double get volume {
    _ensureAlive();
    return native.DspEngineNative.dsp_get_volume(_handle);
  }

  void setVolume(double linearGain) {
    _ensureAlive();
    native.DspEngineNative.dsp_set_volume(_handle, linearGain);
  }

  void setVolumeDb(double db) {
    _ensureAlive();
    native.DspEngineNative.dsp_set_volume_db(_handle, db);
  }

  void setVolumeRamped(double linearGain, {double rampMs = 20.0}) {
    _ensureAlive();
    native.DspEngineNative.dsp_set_volume_ramped(_handle, linearGain, rampMs);
  }

  // ---- Multi-band EQ ----

  bool get eqEnabled {
    _ensureAlive();
    return native.DspEngineNative.dsp_eq_is_enabled(_handle);
  }

  set eqEnabled(bool value) {
    _ensureAlive();
    native.DspEngineNative.dsp_eq_set_enabled(_handle, value);
  }

  int get eqBandCount {
    _ensureAlive();
    return native.DspEngineNative.dsp_eq_band_count(_handle);
  }

  void setEqBand(int index,
      {required double freqHz, required double gainDb, double q = 0}) {
    _ensureAlive();
    native.DspEngineNative.dsp_eq_set_band(_handle, index, freqHz, gainDb, q);
  }

  /// Push studio curve into native EQ. [centersHz] optional (null keeps defaults).
  void setEqBands({List<double>? centersHz, required List<double> gainsDb}) {
    _ensureAlive();
    final n = gainsDb.length;
    final gPtr = calloc<Double>(n);
    Pointer<Double> cPtr = nullptr;
    try {
      for (var i = 0; i < n; i++) {
        gPtr[i] = gainsDb[i];
      }
      if (centersHz != null && centersHz.length >= n) {
        cPtr = calloc<Double>(n);
        for (var i = 0; i < n; i++) {
          cPtr[i] = centersHz[i];
        }
      }
      native.DspEngineNative.dsp_eq_set_bands(_handle, cPtr, gPtr, n);
    } finally {
      calloc.free(gPtr);
      if (cPtr != nullptr) calloc.free(cPtr);
    }
  }

  void resetEq() {
    _ensureAlive();
    native.DspEngineNative.dsp_eq_reset(_handle);
  }

  bool setRealtimePriority([int priority = 1]) {
    _ensureAlive();
    return native.DspEngineNative.dsp_set_realtime_priority(_handle, priority);
  }

  int start() {
    _ensureAlive();
    return native.DspEngineNative.dsp_start(_handle);
  }

  int stop() {
    _ensureAlive();
    return native.DspEngineNative.dsp_stop(_handle);
  }

  void process(Float32List input, Float32List output, int frames) {
    _ensureAlive();
    if (frames <= 0) return;
    final ch = channels;
    final needed = frames * ch;
    if (input.length < needed || output.length < needed) {
      throw DspException(
        'Buffer too small: need $needed samples, '
        'got in=${input.length} out=${output.length}',
      );
    }
    final inPtr = calloc<Float>(needed);
    final outPtr = calloc<Float>(needed);
    try {
      for (var i = 0; i < needed; i++) {
        inPtr[i] = input[i];
      }
      native.DspEngineNative.dsp_process(_handle, inPtr, outPtr, frames);
      for (var i = 0; i < needed; i++) {
        output[i] = outPtr[i];
      }
    } finally {
      calloc.free(inPtr);
      calloc.free(outPtr);
    }
  }

  void processPointers(Pointer<Float> input, Pointer<Float> output, int frames) {
    _ensureAlive();
    native.DspEngineNative.dsp_process(_handle, input, output, frames);
  }

  void dispose() {
    if (_disposed) return;
    _disposed = true;
    if (_handle != nullptr) {
      native.DspEngineNative.dsp_destroy(_handle);
      _handle = nullptr;
    }
  }
}
