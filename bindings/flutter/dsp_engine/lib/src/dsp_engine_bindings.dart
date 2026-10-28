import 'dart:ffi';
import 'dart:io';
import 'package:ffi/ffi.dart';

final class DspConfigNative extends Struct {
  @Float() external double sample_rate;
  @Int32() external int channels;
  @Int32() external int max_block;
  @Int32() external int enable_eq;
  @Int32() external int enable_dvc;
  @Int32() external int enable_limiter;
  @Int32() external int enable_dither;
  @Int32() external int dither_mode;
  @Float() external double limiter_threshold_db;
  @Float() external double limiter_release_ms;
}

final class DspEqBandNative extends Struct {
  @Float() external double freq_hz;
  @Float() external double gain_db;
  @Float() external double q;
  @Int32() external int type;
}

final class DspBackendConfigNative extends Struct {
  @Int32() external int sample_rate;
  @Int32() external int channels;
  @Int32() external int buffer_frames;
  @Int32() external int exclusive;
  @Int32() external int device_id;
}

class DspEngineBindings {
  DspEngineBindings(DynamicLibrary lib) : _lib = lib;
  final DynamicLibrary _lib;

  static DynamicLibrary openLib() {
    if (Platform.isAndroid) return DynamicLibrary.open('libdsp_engine.so');
    if (Platform.isIOS || Platform.isMacOS) {
      return DynamicLibrary.open('dsp_engine.framework/dsp_engine');
    }
    if (Platform.isWindows) return DynamicLibrary.open('dsp_engine.dll');
    if (Platform.isLinux) return DynamicLibrary.open('libdsp_engine.so');
    throw UnsupportedError('Unsupported platform');
  }

  late final Pointer<Utf8> Function() dsp_version =
      _lib.lookupFunction<Pointer<Utf8> Function(), Pointer<Utf8> Function()>('dsp_version');
  late final Pointer<Void> Function(Pointer<DspConfigNative>) dsp_create =
      _lib.lookupFunction<Pointer<Void> Function(Pointer<DspConfigNative>),
          Pointer<Void> Function(Pointer<DspConfigNative>)>('dsp_create');
  late final void Function(Pointer<Void>) dsp_destroy =
      _lib.lookupFunction<Void Function(Pointer<Void>), void Function(Pointer<Void>)>('dsp_destroy');
  late final int Function(Pointer<Void>) dsp_reset =
      _lib.lookupFunction<Int32 Function(Pointer<Void>), int Function(Pointer<Void>)>('dsp_reset');
  late final int Function(Pointer<Void>, double) dsp_set_sample_rate =
      _lib.lookupFunction<Int32 Function(Pointer<Void>, Float), int Function(Pointer<Void>, double)>('dsp_set_sample_rate');
  late final int Function(Pointer<Void>, int) dsp_set_channels =
      _lib.lookupFunction<Int32 Function(Pointer<Void>, Int32), int Function(Pointer<Void>, int)>('dsp_set_channels');
  late final int Function(Pointer<Void>, Pointer<Float>, Pointer<Float>, int) dsp_process =
      _lib.lookupFunction<Int32 Function(Pointer<Void>, Pointer<Float>, Pointer<Float>, Int32),
          int Function(Pointer<Void>, Pointer<Float>, Pointer<Float>, int)>('dsp_process');
  late final int Function(Pointer<Void>, Pointer<Float>, int, Pointer<Float>, Pointer<Int32>, double, int)
      dsp_process_src = _lib.lookupFunction<
          Int32 Function(Pointer<Void>, Pointer<Float>, Int32, Pointer<Float>, Pointer<Int32>, Float, Int32),
          int Function(Pointer<Void>, Pointer<Float>, int, Pointer<Float>, Pointer<Int32>, double, int)>('dsp_process_src');
  late final int Function(Pointer<Void>, int) dsp_eq_enable =
      _lib.lookupFunction<Int32 Function(Pointer<Void>, Int32), int Function(Pointer<Void>, int)>('dsp_eq_enable');
  late final int Function(Pointer<Void>, Pointer<DspEqBandNative>, int) dsp_eq_set_bands =
      _lib.lookupFunction<Int32 Function(Pointer<Void>, Pointer<DspEqBandNative>, Int32),
          int Function(Pointer<Void>, Pointer<DspEqBandNative>, int)>('dsp_eq_set_bands');
  late final int Function(Pointer<Void>, int, Pointer<DspEqBandNative>) dsp_eq_set_band =
      _lib.lookupFunction<Int32 Function(Pointer<Void>, Int32, Pointer<DspEqBandNative>),
          int Function(Pointer<Void>, int, Pointer<DspEqBandNative>)>('dsp_eq_set_band');
  late final int Function(Pointer<Void>) dsp_eq_band_count =
      _lib.lookupFunction<Int32 Function(Pointer<Void>), int Function(Pointer<Void>)>('dsp_eq_band_count');
  late final int Function(Pointer<Void>, double) dsp_dvc_set_gain_db =
      _lib.lookupFunction<Int32 Function(Pointer<Void>, Float), int Function(Pointer<Void>, double)>('dsp_dvc_set_gain_db');
  late final double Function(Pointer<Void>) dsp_dvc_get_gain_db =
      _lib.lookupFunction<Float Function(Pointer<Void>), double Function(Pointer<Void>)>('dsp_dvc_get_gain_db');
  late final int Function(Pointer<Void>, int) dsp_dvc_enable =
      _lib.lookupFunction<Int32 Function(Pointer<Void>, Int32), int Function(Pointer<Void>, int)>('dsp_dvc_enable');
  late final int Function(Pointer<Void>, int) dsp_limiter_enable =
      _lib.lookupFunction<Int32 Function(Pointer<Void>, Int32), int Function(Pointer<Void>, int)>('dsp_limiter_enable');
  late final int Function(Pointer<Void>, double) dsp_limiter_set_threshold =
      _lib.lookupFunction<Int32 Function(Pointer<Void>, Float), int Function(Pointer<Void>, double)>('dsp_limiter_set_threshold');
  late final int Function(Pointer<Void>, double) dsp_limiter_set_release =
      _lib.lookupFunction<Int32 Function(Pointer<Void>, Float), int Function(Pointer<Void>, double)>('dsp_limiter_set_release');
  late final double Function(Pointer<Void>) dsp_limiter_get_threshold =
      _lib.lookupFunction<Float Function(Pointer<Void>), double Function(Pointer<Void>)>('dsp_limiter_get_threshold');
  late final double Function(Pointer<Void>) dsp_limiter_get_release =
      _lib.lookupFunction<Float Function(Pointer<Void>), double Function(Pointer<Void>)>('dsp_limiter_get_release');
  late final int Function(Pointer<Void>) dsp_limiter_is_enabled =
      _lib.lookupFunction<Int32 Function(Pointer<Void>), int Function(Pointer<Void>)>('dsp_limiter_is_enabled');
  late final int Function(Pointer<Void>, int) dsp_dither_enable =
      _lib.lookupFunction<Int32 Function(Pointer<Void>, Int32), int Function(Pointer<Void>, int)>('dsp_dither_enable');
  late final int Function(Pointer<Void>, int) dsp_dither_set_mode =
      _lib.lookupFunction<Int32 Function(Pointer<Void>, Int32), int Function(Pointer<Void>, int)>('dsp_dither_set_mode');
  late final int Function(Pointer<Void>) dsp_dither_get_mode =
      _lib.lookupFunction<Int32 Function(Pointer<Void>), int Function(Pointer<Void>)>('dsp_dither_get_mode');
  late final int Function(Pointer<Void>) dsp_dither_is_enabled =
      _lib.lookupFunction<Int32 Function(Pointer<Void>), int Function(Pointer<Void>)>('dsp_dither_is_enabled');
  late final int Function(Pointer<Void>, int) dsp_set_realtime_priority =
      _lib.lookupFunction<Int32 Function(Pointer<Void>, Int32), int Function(Pointer<Void>, int)>('dsp_set_realtime_priority');
  late final Pointer<Void> Function() dsp_backend_create =
      _lib.lookupFunction<Pointer<Void> Function(), Pointer<Void> Function()>('dsp_backend_create');
  late final void Function(Pointer<Void>) dsp_backend_destroy =
      _lib.lookupFunction<Void Function(Pointer<Void>), void Function(Pointer<Void>)>('dsp_backend_destroy');
  late final int Function(Pointer<Void>, Pointer<DspBackendConfigNative>) dsp_backend_open =
      _lib.lookupFunction<Int32 Function(Pointer<Void>, Pointer<DspBackendConfigNative>),
          int Function(Pointer<Void>, Pointer<DspBackendConfigNative>)>('dsp_backend_open');
  late final void Function(Pointer<Void>) dsp_backend_close =
      _lib.lookupFunction<Void Function(Pointer<Void>), void Function(Pointer<Void>)>('dsp_backend_close');
  late final int Function(Pointer<Void>,
          Pointer<NativeFunction<Void Function(Pointer<Void>, Pointer<Float>, Int32)>>, Pointer<Void>)
      dsp_backend_start = _lib.lookupFunction<
          Int32 Function(Pointer<Void>,
              Pointer<NativeFunction<Void Function(Pointer<Void>, Pointer<Float>, Int32)>>, Pointer<Void>),
          int Function(Pointer<Void>,
              Pointer<NativeFunction<Void Function(Pointer<Void>, Pointer<Float>, Int32)>>,
              Pointer<Void>)>('dsp_backend_start');
  late final int Function(Pointer<Void>) dsp_backend_stop =
      _lib.lookupFunction<Int32 Function(Pointer<Void>), int Function(Pointer<Void>)>('dsp_backend_stop');
  late final int Function(Pointer<Void>) dsp_backend_is_running =
      _lib.lookupFunction<Int32 Function(Pointer<Void>), int Function(Pointer<Void>)>('dsp_backend_is_running');
  late final int Function(Pointer<Void>) dsp_backend_xrun_count =
      _lib.lookupFunction<Int32 Function(Pointer<Void>), int Function(Pointer<Void>)>('dsp_backend_xrun_count');
  late final int Function(Pointer<Void>) dsp_backend_actual_rate =
      _lib.lookupFunction<Int32 Function(Pointer<Void>), int Function(Pointer<Void>)>('dsp_backend_actual_rate');
  late final int Function(Pointer<Void>) dsp_backend_actual_channels =
      _lib.lookupFunction<Int32 Function(Pointer<Void>), int Function(Pointer<Void>)>('dsp_backend_actual_channels');
  late final int Function(Pointer<Void>) dsp_backend_actual_buffer_frames =
      _lib.lookupFunction<Int32 Function(Pointer<Void>), int Function(Pointer<Void>)>('dsp_backend_actual_buffer_frames');
}
