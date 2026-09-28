# dsp_engine (Flutter FFI)

Flutter bindings for **[DSP ENGINE](https://github.com/innotrepid/DSP-ENGINE)** — 64-bit audio processing with Direct Volume Control (DVC).

## App identity

This package is a **library**, not an application.

| | Value |
|--|--------|
| Resonate applicationId | `com.aetherion.resonate` / `com.Aetherion.Resonate` |
| This package | `dsp_engine` (pubspec name only) |
| Separate DSP app id? | **No** — do not use `com.aetherion.resonatedsp` for the player |

When integrated into Resonate, native code runs inside Resonate’s process and package name.

## Usage

```yaml
dependencies:
  dsp_engine:
    path: path/to/DSP-ENGINE/bindings/flutter/dsp_engine
```

```dart
import 'package:dsp_engine/dsp_engine.dart';

final engine = DspEngine.create(DspConfig.mobile());
engine.setVolumeDb(-6.0);
engine.process(input, output, frameCount);
engine.dispose();
```

## Native library

Ship `libdsp_engine` built from the CMake project at the repo root.

## Status

Skeleton / Phase 0 — high-level Dart API + manual FFI bindings.
