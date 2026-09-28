/// Configuration for a [DspEngine] instance.
class DspConfig {
  /// Sample rate in Hz (e.g. 44100, 48000, 96000).
  final int sampleRate;

  /// Channel count (1 = mono, 2 = stereo; up to 8 supported by the engine).
  final int channels;

  /// Preferred callback / process block size in frames.
  final int bufferFrames;

  /// Request exclusive / low-latency backend when available.
  final bool exclusiveMode;

  /// Enable bit-perfect unity-gain path when no processing is active.
  final bool bitPerfect;

  /// 0 = normal priority; >0 attempts near-real-time priority.
  final int realtimePriority;

  const DspConfig({
    this.sampleRate = 48000,
    this.channels = 2,
    this.bufferFrames = 256,
    this.exclusiveMode = true,
    this.bitPerfect = true,
    this.realtimePriority = 1,
  });

  /// Sensible defaults for mobile music playback.
  factory DspConfig.mobile() => const DspConfig(
        sampleRate: 48000,
        channels: 2,
        bufferFrames: 256,
        exclusiveMode: true,
        bitPerfect: true,
        realtimePriority: 1,
      );

  /// Lower latency preset (may increase CPU / drop risk on weak devices).
  factory DspConfig.lowLatency() => const DspConfig(
        sampleRate: 48000,
        channels: 2,
        bufferFrames: 128,
        exclusiveMode: true,
        bitPerfect: true,
        realtimePriority: 1,
      );
}
