/**
 * Minimal example: create engine, process a buffer with DVC, destroy.
 */

#include "dsp_engine.h"
#include <cstdio>
#include <vector>
#include <cmath>

int main()
{
    DspConfig cfg{};
    cfg.sample_rate = 48000.f;
    cfg.channels = 2;
    cfg.max_block = 1024;
    cfg.enable_eq = 0;
    cfg.enable_dvc = 1;
    cfg.enable_limiter = 0;
    cfg.enable_dither = 0;
    cfg.dither_mode = DSP_DITHER_NONE;
    cfg.limiter_threshold_db = -1.0f;
    cfg.limiter_release_ms = 50.f;

    DspEngine* eng = dsp_create(&cfg);
    if (!eng) {
        std::printf("Failed to create engine\n");
        return 1;
    }

    std::printf("DSP ENGINE %s created\n", dsp_version());
    std::printf("  sample rate : %.0f\n", cfg.sample_rate);
    std::printf("  channels    : %d\n", cfg.channels);

    // Generate a simple 440 Hz sine wave (float32 interleaved)
    const int frames = 256;
    std::vector<float> in(static_cast<size_t>(frames) * 2);
    std::vector<float> out(static_cast<size_t>(frames) * 2);

    for (int i = 0; i < frames; ++i) {
        float t = static_cast<float>(i) / cfg.sample_rate;
        float s = 0.5f * std::sin(2.0f * 3.14159265f * 440.0f * t);
        in[static_cast<size_t>(i) * 2]     = s;
        in[static_cast<size_t>(i) * 2 + 1] = s;
    }

    // Apply -6 dB via Direct Volume Control
    dsp_dvc_set_gain_db(eng, -6.0f);

    DspError err = dsp_process(eng, in.data(), out.data(), frames);
    if (err != DSP_OK) {
        std::printf("dsp_process failed: %d\n", static_cast<int>(err));
        dsp_destroy(eng);
        return 1;
    }

    std::printf("Processed %d frames with volume = %.3f dB\n",
                frames, dsp_dvc_get_gain_db(eng));
    std::printf("First output sample L/R: %.6f / %.6f\n",
                out[0], out[1]);

    dsp_destroy(eng);
    std::printf("Done.\n");
    return 0;
}
