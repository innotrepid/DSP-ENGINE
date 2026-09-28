# Contributing to DSP ENGINE

Thank you for your interest in improving DSP ENGINE.

## Development setup

```bash
git clone https://github.com/innotrepid/DSP-ENGINE.git
cd DSP-ENGINE
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DDSP_BUILD_EXAMPLES=ON
cmake --build .
./examples/dsp_example_offline
```

## Coding guidelines

- **C++17** for implementation, **C11** for the public ABI.
- Public API must remain pure C (`extern "C"`) so it can be consumed from Flutter, Rust, Python, etc.
- Prefer clear, readable code over micro-optimizations unless you are working in the real-time path.
- Real-time audio callbacks must be lock-free and allocation-free.
- Use 64-bit (`double`) for internal processing unless there is a measured reason not to.
- Document any new public functions in `include/dsp_engine.h`.

## Pull requests

1. Fork the repository and create a feature branch.
2. Keep commits focused and well-described.
3. Make sure the offline example still runs.
4. Open a pull request against `main` and describe the change.

## Areas that need help

- Platform audio backends (AAudio exclusive, WASAPI exclusive, Core Audio, PipeWire)
- High-quality sample-rate conversion
- Parametric / graphic EQ in the 64-bit domain
- True-peak limiter + noise-shaped dither
- Flutter / Dart FFI package
- Benchmarks and latency measurements
- Unit tests

## License

By contributing you agree that your contributions will be licensed under the MIT License.
