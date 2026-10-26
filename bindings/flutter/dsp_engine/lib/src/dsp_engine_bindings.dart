import 'dart:ffi';
import 'package:ffi/ffi.dart';

// Native struct mirrors (must match include/dsp_engine.h)
final class DspConfigNative extends Struct {
  @Float()
  external double sampleRate;
  @Int32()
  external int channels;
  @Int32()
  external int maxBlock;
  @Int32()
  external int enableEq;
  @Int32()
  external int enableDvc;
  @Int32()
  external int enableLimiter;
  @Int32()
  external int enableDither;
  @Int32()
  external int ditherMode;
  @Float()
  external double limiterThresholdDb;
  @Float()
  external double limiterReleaseMs;
}

final class DspEqBandNative extends Struct {
  @Float()
  external double freqHz;
  @Float()
  external double gainDb;
  @Float()
  external double q;
  @Int32()
  external int type;
}

class DspEngineBindings {
  late final DynamicLibrary _lib;

  DspEngineBindings() {
    _lib = DynamicLibrary.open(_libName);
  }

  static String get _libName {
    // Platform-specific library name
    // On Android the .so is loaded by the plugin; adjust as needed.
    return 'libdsp_engine.so';
  }

  late final Pointer<Utf8> Function() dsp_version =
      _lib.lookupFunction<Pointer<Utf8> Function(), Pointer<Utf8> Function()>('dsp_version');

  late final Pointer<Void> Function(Pointer<DspConfigNative>) dsp_create =
      _lib.lookupFunction<
          Pointer<Void> Function(Pointer<DspConfigNative>),
          Pointer<Void> Function(Pointer<DspConfigNative>)>('dsp_create');

  late final void Function(Pointer<Void>) dsp_destroy =
      _lib.lookupFunction<Void Function(Pointer<Void>), void Function(Pointer<Void>)>('dsp_destroy');

  late final int Function(Pointer<Void>) dsp_reset =
      _lib.lookupFunction<Int32 Function(Pointer<Void>), int Function(Pointer<Void>)>('dsp_reset');

  late final int Function(Pointer<Void>, double) dsp_set_sample_rate =
      _lib.lookupFunction<
          Int32 Function(Pointer<Void>, Float),
          int Function(Pointer<Void>, double)>('dsp_set_sample_rate');

  late final int Function(Pointer<Void>, int) dsp_set_channels =
      _lib.lookupFunction<
          Int32 Function(Pointer<Void>, Int32),
          int Function(Pointer<Void>, int)>('dsp_set_channels');

  late final int Function(Pointer<Void>, Pointer<Float>, Pointer<Float>, int) dsp_process =
      _lib.lookupFunction<
          Int32 Function(Pointer<Void>, Pointer<Float>, Pointer<Float>, Int32),
          int Function(Pointer<Void>, Pointer<Float>, Pointer<Float>, int)>('dsp_process');

  late final int Function(Pointer<Void>, int) dsp_eq_enable =
      _lib.lookupFunction<
          Int32 Function(Pointer<Void>, Int32),
          int Function(Pointer<Void>, int)>('dsp_eq_enable');

  late final int Function(Pointer<Void>, Pointer<DspEqBandNative>, int) dsp_eq_set_bands =
      _lib.lookupFunction<
          Int32 Function(Pointer<Void>, Pointer<DspEqBandNative>, Int32),
          int Function(Pointer<Void>, Pointer<DspEqBandNative>, int)>('dsp_eq_set_bands');

  late final int Function(Pointer<Void>, double) dsp_dvc_set_gain_db =
      _lib.lookupFunction<
          Int32 Function(Pointer<Void>, Float),
          int Function(Pointer<Void>, double)>('dsp_dvc_set_gain_db');

  late final double Function(Pointer<Void>) dsp_dvc_get_gain_db =
      _lib.lookupFunction<
          Float Function(Pointer<Void>),
          double Function(Pointer<Void>)>('dsp_dvc_get_gain_db');

  late final int Function(Pointer<Void>, int) dsp_limiter_enable =
      _lib.lookupFunction<
          Int32 Function(Pointer<Void>, Int32),
          int Function(Pointer<Void>, int)>('dsp_limiter_enable');

  late final int Function(Pointer<Void>, double) dsp_limiter_set_threshold =
      _lib.lookupFunction<
          Int32 Function(Pointer<Void>, Float),
          int Function(Pointer<Void>, double)>('dsp_limiter_set_threshold');

  late final int Function(Pointer<Void>, double) dsp_limiter_set_release =
      _lib.lookupFunction<
          Int32 Function(Pointer<Void>, Float),
          int Function(Pointer<Void>, double)>('dsp_limiter_set_release');

  late final int Function(Pointer<Void>, int) dsp_dither_enable =
      _lib.lookupFunction<
          Int32 Function(Pointer<Void>, Int32),
          int Function(Pointer<Void>, int)>('dsp_dither_enable');

  late final int Function(Pointer<Void>, int) dsp_dither_set_mode =
      _lib.lookupFunction<
          Int32 Function(Pointer<Void>, Int32),
          int Function(Pointer<Void>, int)>('dsp_dither_set_mode');
}
