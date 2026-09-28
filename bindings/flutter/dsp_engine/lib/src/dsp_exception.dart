/// Thrown when the native DSP ENGINE reports an error.
class DspException implements Exception {
  final String message;
  final int? code;

  const DspException(this.message, [this.code]);

  @override
  String toString() =>
      code != null ? 'DspException($code): $message' : 'DspException: $message';
}
