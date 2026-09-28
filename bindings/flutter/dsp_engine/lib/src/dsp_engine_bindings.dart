// ignore_for_file: non_constant_identifier_names, camel_case_types

import 'dart:ffi';
import 'dart:io';

import 'package:ffi/ffi.dart' show Utf8;

/// Low-level FFI bindings matching `include/dsp_engine.h`.
class DspEngineNative {
  DspEngineNative._();

  static DynamicLibrary? _lib;

  static DynamicLibrary get lib {
    return _lib ??= _open();
  }

  static DynamicLibrary _open() {
    if (Platform.isAndroid) {
      return DynamicLibrary.open('libdsp_engine.so');
    }
    if (Platform.isIOS || Platform.isMacOS) {
      return DynamicLibrary.process();
    }
    if (Platform.isLinux) {
      return DynamicLibrary.open('libdsp_engine.so');
    }
    if (Platform.isWindows) {
      return DynamicLibrary.open('dsp_engine.dll');
    }
    throw UnsupportedError('Unsupported platform for DSP ENGINE');
  }

  static final dsp_create = lib.lookupFunction<
      Pointer<Void> Function(Pointer<DspConfigNative>),
      Pointer<Void> Function(Pointer<DspConfigNative>)>('dsp_create');

  static final dsp_destroy = lib.lookupFunction<
      Void Function(Pointer<Void>),
      void Function(Pointer<Void>)>('dsp_destroy');

  static final dsp_start = lib.lookupFunction<
      Int32 Function(Pointer<Void>),
      int Function(Pointer<Void>)>('dsp_start');

  static final dsp_stop = lib.lookupFunction<
      Int32 Function(Pointer<Void>),
      int Function(Pointer<Void>)>('dsp_stop');

  static final dsp_process = lib.lookupFunction<
      Void Function(Pointer<Void>, Pointer<Float>, Pointer<Float>, Int32),
      void Function(Pointer<Void>, Pointer<Float>, Pointer<Float>, int)>(
    'dsp_process',
  );

  static final dsp_set_volume = lib.lookupFunction<
      Void Function(Pointer<Void>, Double),
      void Function(Pointer<Void>, double)>('dsp_set_volume');

  static final dsp_get_volume = lib.lookupFunction<
      Double Function(Pointer<Void>),
      double Function(Pointer<Void>)>('dsp_get_volume');

  static final dsp_set_volume_db = lib.lookupFunction<
      Void Function(Pointer<Void>, Double),
      void Function(Pointer<Void>, double)>('dsp_set_volume_db');

  static final dsp_set_volume_ramped = lib.lookupFunction<
      Void Function(Pointer<Void>, Double, Double),
      void Function(Pointer<Void>, double, double)>('dsp_set_volume_ramped');

  static final dsp_set_realtime_priority = lib.lookupFunction<
      Bool Function(Pointer<Void>, Int32),
      bool Function(Pointer<Void>, int)>('dsp_set_realtime_priority');

  static final dsp_is_running = lib.lookupFunction<
      Bool Function(Pointer<Void>),
      bool Function(Pointer<Void>)>('dsp_is_running');

  static final dsp_get_sample_rate = lib.lookupFunction<
      Int32 Function(Pointer<Void>),
      int Function(Pointer<Void>)>('dsp_get_sample_rate');

  static final dsp_get_channels = lib.lookupFunction<
      Int32 Function(Pointer<Void>),
      int Function(Pointer<Void>)>('dsp_get_channels');

  static final dsp_get_buffer_frames = lib.lookupFunction<
      Int32 Function(Pointer<Void>),
      int Function(Pointer<Void>)>('dsp_get_buffer_frames');

  static final dsp_get_processed_frames = lib.lookupFunction<
      Uint64 Function(Pointer<Void>),
      int Function(Pointer<Void>)>('dsp_get_processed_frames');

  static final dsp_get_xrun_count = lib.lookupFunction<
      Uint32 Function(Pointer<Void>),
      int Function(Pointer<Void>)>('dsp_get_xrun_count');

  static final dsp_last_error = lib.lookupFunction<
      Pointer<Utf8> Function(),
      Pointer<Utf8> Function()>('dsp_last_error');

  static final dsp_version = lib.lookupFunction<
      Pointer<Utf8> Function(),
      Pointer<Utf8> Function()>('dsp_version');
}

/// Native layout of C `DspConfig` (must match `dsp_engine.h`).
final class DspConfigNative extends Struct {
  @Int32()
  external int sample_rate;

  @Int32()
  external int channels;

  @Int32()
  external int buffer_frames;

  @Bool()
  external bool exclusive_mode;

  @Bool()
  external bool bit_perfect;

  @Int32()
  external int realtime_priority;
}
