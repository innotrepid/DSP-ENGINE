import 'dart:ffi';
import 'dart:typed_data';
import 'package:ffi/ffi.dart';

import 'dsp_engine_bindings.dart';
import 'dsp_exception.dart';

/// High-level wrapper around the DSP ENGINE C ABI.
class DspEngine {
  DspEngine._(this._b, this._handle);

  final DspEngineBindings _b;
  final Pointer<Void> _handle;
  bool _disposed = false;

  /// Open the native library and create an engine.
  static DspEngine create({
    double sampleRate = 48000,
    int channels = 2,
    int maxBlock = 1024,
    bool enableEq = false,
    bool enableDvc = true,
    bool enableLimiter = false,
    bool enableDither = false,
    DitherMode ditherMode = DitherMode.none,
    double limiterThresholdDb = -1.0,
    double limiterReleaseMs = 50.0,
    DynamicLibrary? lib,
  }) {
    final b = DspEngineBindings(lib ?? DspEngineBindings.openLib());
    final cfg = calloc<DspConfigNative>();
    cfg.ref
      ..sample_rate = sampleRate
      ..channels = channels
      ..max_block = maxBlock
      ..enable_eq = enableEq ? 1 : 0
      ..enable_dvc = enableDvc ? 1 : 0
      ..enable_limiter = enableLimiter ? 1 : 0
      ..enable_dither = enableDither ? 1 : 0
      ..dither_mode = ditherMode.index
      ..limiter_threshold_db = limiterThresholdDb
      ..limiter_release_ms = limiterReleaseMs;
    final h = b.dsp_create(cfg);
    calloc.free(cfg);
    if (h == nullptr) {
      throw DspException('dsp_create failed');
    }
    return DspEngine._(b, h);
  }

  String get version {
    final p = _b.dsp_version();
    return p == nullptr ? '' : p.toDartString();
  }

  void reset() => _check(_b.dsp_reset(_handle));

  void setSampleRate(double rate) => _check(_b.dsp_set_sample_rate(_handle, rate));

  void setChannels(int channels) => _check(_b.dsp_set_channels(_handle, channels));

  /// Process [frames] interleaved frames from [input] into [output].
  void processFrames(Float32List input, Float32List output, int frames) {
    _ensure();
    final inPtr = calloc<Float>(input.length);
    final outPtr = calloc<Float>(output.length);
    inPtr.asTypedList(input.length).setAll(0, input);
    _check(_b.dsp_process(_handle, inPtr, outPtr, frames));
    output.setAll(0, outPtr.asTypedList(output.length));
    calloc.free(inPtr);
    calloc.free(outPtr);
  }

  /// Resample then run the full DSP pipeline (EQ → DVC → limiter → dither).
  ///
  /// [ratio] ≈ sourceRate / destRate (`< 1` upsamples, `> 1` downsamples).
  /// [output] must hold at least [maxOutFrames] * channels interleaved samples.
  /// Returns frames written.
  int processSrc(
    Float32List input,
    int inFrames,
    Float32List output,
    int maxOutFrames, {
    required int channels,
    required double ratio,
    SrcQuality quality = SrcQuality.cubic,
  }) {
    _ensure();
    final inPtr = calloc<Float>(input.length);
    final outPtr = calloc<Float>(output.length);
    final outFrames = calloc<Int32>();
    outFrames.value = maxOutFrames;
    inPtr.asTypedList(input.length).setAll(0, input);
    _check(_b.dsp_process_src(
        _handle, inPtr, inFrames, outPtr, outFrames, ratio, quality.index));
    final written = outFrames.value;
    final n = (written * channels).clamp(0, output.length);
    output.setAll(0, outPtr.asTypedList(n));
    calloc.free(inPtr);
    calloc.free(outPtr);
    calloc.free(outFrames);
    return written;
  }

  void setEqEnabled(bool enable) => _check(_b.dsp_eq_enable(_handle, enable ? 1 : 0));

  void setEqBands(List<EqBand> bands) {
    if (bands.isEmpty) {
      _check(_b.dsp_eq_set_bands(_handle, nullptr, 0));
      return;
    }
    final ptr = calloc<DspEqBandNative>(bands.length);
    for (var i = 0; i < bands.length; i++) {
      ptr[i]
        ..freq_hz = bands[i].freqHz
        ..gain_db = bands[i].gainDb
        ..q = bands[i].q
        ..type = bands[i].type.index;
    }
    _check(_b.dsp_eq_set_bands(_handle, ptr, bands.length));
    calloc.free(ptr);
  }

  void setEqBand(int index, EqBand band) {
    final ptr = calloc<DspEqBandNative>();
    ptr.ref
      ..freq_hz = band.freqHz
      ..gain_db = band.gainDb
      ..q = band.q
      ..type = band.type.index;
    _check(_b.dsp_eq_set_band(_handle, index, ptr));
    calloc.free(ptr);
  }

  int get eqBandCount => _b.dsp_eq_band_count(_handle);

  void setDvcGainDb(double db) => _check(_b.dsp_dvc_set_gain_db(_handle, db));

  double get dvcGainDb => _b.dsp_dvc_get_gain_db(_handle);

  void setDvcEnabled(bool enable) => _check(_b.dsp_dvc_enable(_handle, enable ? 1 : 0));

  void setLimiterEnabled(bool enable) =>
      _check(_b.dsp_limiter_enable(_handle, enable ? 1 : 0));

  void setLimiterThresholdDb(double db) =>
      _check(_b.dsp_limiter_set_threshold(_handle, db));

  void setLimiterReleaseMs(double ms) =>
      _check(_b.dsp_limiter_set_release(_handle, ms));

  double get limiterThresholdDb => _b.dsp_limiter_get_threshold(_handle);
  double get limiterReleaseMs => _b.dsp_limiter_get_release(_handle);
  bool get limiterEnabled => _b.dsp_limiter_is_enabled(_handle) != 0;

  void setDitherEnabled(bool enable) =>
      _check(_b.dsp_dither_enable(_handle, enable ? 1 : 0));

  void setDitherMode(DitherMode mode) =>
      _check(_b.dsp_dither_set_mode(_handle, mode.index));

  DitherMode get ditherMode {
    final i = _b.dsp_dither_get_mode(_handle);
    if (i < 0 || i >= DitherMode.values.length) return DitherMode.none;
    return DitherMode.values[i];
  }

  bool get ditherEnabled => _b.dsp_dither_is_enabled(_handle) != 0;

  /// Raise the current thread toward real-time priority. Returns true on success.
  bool setRealtimePriority(int priority) =>
      _b.dsp_set_realtime_priority(_handle, priority) != 0;

  void dispose() {
    if (_disposed) return;
    _b.dsp_destroy(_handle);
    _disposed = true;
  }

  void _ensure() {
    if (_disposed) throw StateError('DspEngine disposed');
  }

  void _check(int code) {
    if (code != 0) throw DspException('DSP error $code');
  }
}

/// Platform audio output backend (AAudio / WASAPI / Core Audio / ALSA / null).
class DspBackend {
  DspBackend._(this._b, this._handle);

  final DspEngineBindings _b;
  final Pointer<Void> _handle;
  bool _disposed = false;
  NativeCallable<Void Function(Pointer<Void>, Pointer<Float>, Int32)>? _renderCallable;

  static DspBackend create({DynamicLibrary? lib}) {
    final b = DspEngineBindings(lib ?? DspEngineBindings.openLib());
    final h = b.dsp_backend_create();
    if (h == nullptr) throw DspException('dsp_backend_create failed');
    return DspBackend._(b, h);
  }

  void open({
    int sampleRate = 48000,
    int channels = 2,
    int bufferFrames = 256,
    bool exclusive = true,
    int deviceId = 0,
  }) {
    _ensure();
    final cfg = calloc<DspBackendConfigNative>();
    cfg.ref
      ..sample_rate = sampleRate
      ..channels = channels
      ..buffer_frames = bufferFrames
      ..exclusive = exclusive ? 1 : 0
      ..device_id = deviceId;
    final err = _b.dsp_backend_open(_handle, cfg);
    calloc.free(cfg);
    _check(err);
  }

  void close() {
    if (!_disposed) _b.dsp_backend_close(_handle);
  }

  /// Start playback. [onRender] receives interleaved floats on the audio thread.
  /// Keep it realtime-safe (no Dart heap alloc / locks / async).
  void start(void Function(Pointer<Float> out, int frames) onRender) {
    _ensure();
    _renderCallable?.close();
    _renderCallable =
        NativeCallable<Void Function(Pointer<Void>, Pointer<Float>, Int32)>.listener(
      (Pointer<Void> user, Pointer<Float> out, int frames) {
        onRender(out, frames);
      },
    );
    _check(_b.dsp_backend_start(_handle, _renderCallable!.nativeFunction, nullptr));
  }

  /// Start with a raw native function pointer.
  void startNative(
    Pointer<NativeFunction<Void Function(Pointer<Void>, Pointer<Float>, Int32)>> cb,
    Pointer<Void> user,
  ) {
    _ensure();
    _check(_b.dsp_backend_start(_handle, cb, user));
  }

  void stop() => _check(_b.dsp_backend_stop(_handle));

  bool get isRunning => _b.dsp_backend_is_running(_handle) != 0;
  int get xrunCount => _b.dsp_backend_xrun_count(_handle);
  int get actualRate => _b.dsp_backend_actual_rate(_handle);
  int get actualChannels => _b.dsp_backend_actual_channels(_handle);
  int get actualBufferFrames => _b.dsp_backend_actual_buffer_frames(_handle);

  void dispose() {
    if (_disposed) return;
    try {
      stop();
    } catch (_) {}
    _renderCallable?.close();
    _renderCallable = null;
    _b.dsp_backend_close(_handle);
    _b.dsp_backend_destroy(_handle);
    _disposed = true;
  }

  void _ensure() {
    if (_disposed) throw StateError('DspBackend disposed');
  }

  void _check(int code) {
    if (code != 0) throw DspException('DSP backend error $code');
  }
}

enum DitherMode { none, tpdf, shaped }

enum SrcQuality { none, linear, cubic, hq }

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
