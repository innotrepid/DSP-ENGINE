/**
 * Minimal example: create engine, process a buffer with DVC, destroy.
 */

#include "dsp_engine.h"
#include <cstdio>
#include <vector>
#include <cmath>

int main()
{
    DspConfig cfg = dsp_config_default();
    cfg.sample_rate   = 48000;
    cfg.channels      = 2;
    cfg.buffer_frames = 256;

    DspEngine* eng = dsp_create(&cfg);
    if (!eng) {
        std::printf("Failed to create engine: %s\n", dsp_last_error());
        return 1;
    }

    std::printf("DSP ENGINE %s created\n", dsp_version());
    std::printf("  sample rate : %d\n", dsp_get_sample_rate(eng));
    std::printf("  channels    : %d\n", dsp_get_channels(eng));

    // Generate a simple 440 Hz sine wave (float32 interleaved)
    const int frames = 256;
    std::vector<float> in(frames * 2);
    std::vector<float> out(frames * 2);

    for (int i = 0; i < frames; ++i) {
        float t = static_cast<float>(i) / cfg.sample_rate;
        float s = 0.5f * std::sin(2.0f * 3.14159265f * 440.0f * t);
        in[i * 2]     = s;
        in[i * 2 + 1] = s;
    }

    // Apply -6 dB via Direct Volume Control
    dsp_set_volume_db(eng, -6.0);

    dsp_process(eng, in.data(), out.data(), frames);

    std::printf("Processed %d frames with volume = %.3f\n",
                frames, dsp_get_volume(eng));
    std::printf("First output sample L/R: %.6f / %.6f\n",
                out[0], out[1]);

    dsp_destroy(eng);
    std::printf("Done.\n");
    return 0;
}
