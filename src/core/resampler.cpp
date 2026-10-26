/**
 * Sample-rate conversion helpers used by dsp_process_src.
 * Linear, cubic (Catmull-Rom) and a simple windowed-sinc HQ path.
 */

#include "dsp_engine.h"
#include <cmath>
#include <cstring>
#include <vector>

namespace dsp {

// Simple blackman windowed sinc kernel (odd length)
static void make_sinc_kernel(std::vector<float>& kernel, int taps, float cutoff) {
    kernel.resize(taps);
    int mid = taps / 2;
    double sum = 0.0;
    for (int i = 0; i < taps; ++i) {
        double x = i - mid;
        double sinc = (x == 0.0) ? 1.0 : std::sin(M_PI * cutoff * x) / (M_PI * x);
        // Blackman
        double w = 0.42 - 0.5 * std::cos(2.0 * M_PI * i / (taps - 1))
                        + 0.08 * std::cos(4.0 * M_PI * i / (taps - 1));
        kernel[i] = static_cast<float>(sinc * w);
        sum += kernel[i];
    }
    if (sum != 0.0) {
        for (float& v : kernel) v = static_cast<float>(v / sum);
    }
}

} // namespace dsp

// The actual SRC logic lives inside processor.cpp for simplicity in this release.
// This file provides the HQ kernel builder and any future polyphase tables.

extern "C" {

// Placeholder so the object is always present in the library.
void dsp_resampler_init_placeholder(void) {}

} // extern "C"
