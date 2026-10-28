/**
 * Unit tests for DSP ENGINE v0.3.2
 */

#include "test_framework.h"
#include "dsp_engine.h"
#include "audio/ring_buffer.h"
#include "utils/math_utils.h"
#include "core/resampler.h"
#include <vector>
#include <cmath>
#include <cstring>

using namespace dsp_test;

static DspEngine* create_engine(int channels = 2, float rate = 48000.f) {
    DspConfig cfg{};
    cfg.sample_rate = rate;
    cfg.channels = channels;
    cfg.max_block = 1024;
    cfg.enable_eq = 1;
    cfg.enable_dvc = 1;
    cfg.enable_limiter = 1;
    cfg.enable_dither = 0;
    cfg.dither_mode = DSP_DITHER_TPDF;
    cfg.limiter_threshold_db = -1.0f;
    cfg.limiter_release_ms = 50.f;
    return dsp_create(&cfg);
}

TEST(version) {
    const char* v = dsp_version();
    EXPECT_TRUE(v != nullptr);
    EXPECT_TRUE(std::strstr(v, "0.3") != nullptr);
}

TEST(lifecycle) {
    DspEngine* eng = create_engine();
    EXPECT_TRUE(eng != nullptr);
    EXPECT_EQ(dsp_reset(eng), DSP_OK);
    dsp_destroy(eng);
}

TEST(invalid_create) {
    DspConfig cfg{};
    cfg.channels = 0;
    cfg.max_block = 1024;
    EXPECT_TRUE(dsp_create(&cfg) == nullptr);
}

TEST(dvc_gain) {
    DspEngine* eng = create_engine();
    EXPECT_EQ(dsp_dvc_set_gain_db(eng, -6.0f), DSP_OK);
    EXPECT_NEAR(dsp_dvc_get_gain_db(eng), -6.0f, 1e-5);

    const int frames = 256;
    std::vector<float> in(frames * 2, 0.5f);
    std::vector<float> out(frames * 2);
    EXPECT_EQ(dsp_process(eng, in.data(), out.data(), frames), DSP_OK);
    EXPECT_NEAR(out[0], 0.5f * 0.501187f, 0.01f);
    dsp_destroy(eng);
}

TEST(eq_bands) {
    DspEngine* eng = create_engine();
    DspEqBand bands[2] = {
        {1000.f, 6.0f, 1.0f, 0},
        {5000.f, -3.0f, 0.7f, 0}
    };
    EXPECT_EQ(dsp_eq_set_bands(eng, bands, 2), DSP_OK);
    EXPECT_EQ(dsp_eq_band_count(eng), 2);
    EXPECT_EQ(dsp_eq_enable(eng, 1), DSP_OK);

    const int frames = 512;
    std::vector<float> in(frames * 2, 0.1f);
    std::vector<float> out(frames * 2);
    EXPECT_EQ(dsp_process(eng, in.data(), out.data(), frames), DSP_OK);
    EXPECT_TRUE(std::isfinite(out[0]));
    dsp_destroy(eng);
}

TEST(limiter) {
    DspEngine* eng = create_engine();
    EXPECT_EQ(dsp_limiter_enable(eng, 1), DSP_OK);
    EXPECT_EQ(dsp_limiter_set_threshold(eng, -6.0f), DSP_OK);
    dsp_eq_enable(eng, 0);
    dsp_dvc_set_gain_db(eng, 0.f);

    const int frames = 1024;
    std::vector<float> in(frames * 2, 0.9f);
    std::vector<float> out(frames * 2);
    EXPECT_EQ(dsp_process(eng, in.data(), out.data(), frames), DSP_OK);
    EXPECT_EQ(dsp_process(eng, in.data(), out.data(), frames), DSP_OK);

    float peak = 0.f;
    for (float s : out) {
        float a = std::fabs(s);
        if (a > peak) peak = a;
    }
    EXPECT_LT(peak, 0.65f);
    dsp_destroy(eng);
}

TEST(dither) {
    DspEngine* eng = create_engine();
    dsp_eq_enable(eng, 0);
    dsp_limiter_enable(eng, 0);
    dsp_dvc_set_gain_db(eng, 0.f);
    EXPECT_EQ(dsp_dither_enable(eng, 1), DSP_OK);
    EXPECT_EQ(dsp_dither_set_mode(eng, DSP_DITHER_TPDF), DSP_OK);

    const int frames = 256;
    std::vector<float> in(frames * 2, 0.25f);
    std::vector<float> out(frames * 2);
    EXPECT_EQ(dsp_process(eng, in.data(), out.data(), frames), DSP_OK);
    EXPECT_TRUE(std::isfinite(out[0]));
    EXPECT_NEAR(out[0], 0.25f, 3.0f / 32768.f);
    dsp_destroy(eng);
}

TEST(src_linear) {
    DspEngine* eng = create_engine();
    const int in_frames = 128;
    std::vector<float> in(in_frames * 2, 0.2f);
    std::vector<float> out(256 * 2);
    int out_frames = 256;
    EXPECT_EQ(dsp_process_src(eng, in.data(), in_frames, out.data(), &out_frames, 0.5f, DSP_SRC_LINEAR), DSP_OK);
    EXPECT_GT(out_frames, 0);
    dsp_destroy(eng);
}

TEST(src_cubic) {
    DspEngine* eng = create_engine();
    const int in_frames = 128;
    std::vector<float> in(in_frames * 2, 0.2f);
    std::vector<float> out(256 * 2);
    int out_frames = 256;
    EXPECT_EQ(dsp_process_src(eng, in.data(), in_frames, out.data(), &out_frames, 1.5f, DSP_SRC_CUBIC), DSP_OK);
    EXPECT_GT(out_frames, 0);
    dsp_destroy(eng);
}

TEST(src_pipeline) {
    DspEngine* eng = create_engine();
    dsp_eq_enable(eng, 0);
    dsp_limiter_enable(eng, 0);
    dsp_dither_enable(eng, 0);
    dsp_dvc_enable(eng, 1);
    EXPECT_EQ(dsp_dvc_set_gain_db(eng, -6.0f), DSP_OK);

    const int in_frames = 64;
    std::vector<float> in(in_frames * 2, 0.5f);
    std::vector<float> out(128 * 2);
    int out_frames = 128;
    EXPECT_EQ(dsp_process_src(eng, in.data(), in_frames, out.data(), &out_frames, 1.0f, DSP_SRC_LINEAR), DSP_OK);
    EXPECT_GT(out_frames, 0);
    EXPECT_NEAR(out[0], 0.5f * 0.501187f, 0.02f);
    dsp_destroy(eng);
}

TEST(limiter_getters) {
    DspEngine* eng = create_engine();
    EXPECT_EQ(dsp_limiter_set_threshold(eng, -3.0f), DSP_OK);
    EXPECT_EQ(dsp_limiter_set_release(eng, 80.f), DSP_OK);
    EXPECT_EQ(dsp_limiter_enable(eng, 1), DSP_OK);
    EXPECT_NEAR(dsp_limiter_get_threshold(eng), -3.0f, 1e-5);
    EXPECT_NEAR(dsp_limiter_get_release(eng), 80.f, 1e-5);
    EXPECT_EQ(dsp_limiter_is_enabled(eng), 1);
    dsp_destroy(eng);
}

TEST(dither_getters) {
    DspEngine* eng = create_engine();
    EXPECT_EQ(dsp_dither_set_mode(eng, DSP_DITHER_SHAPED), DSP_OK);
    EXPECT_EQ(dsp_dither_enable(eng, 1), DSP_OK);
    EXPECT_EQ(dsp_dither_get_mode(eng), DSP_DITHER_SHAPED);
    EXPECT_EQ(dsp_dither_is_enabled(eng), 1);
    dsp_destroy(eng);
}

TEST(math_db) {
    EXPECT_NEAR(dsp::db_to_linear(0.0), 1.0, 1e-9);
    EXPECT_NEAR(dsp::db_to_linear(-6.0), 0.501187, 1e-4);
    EXPECT_NEAR(dsp::linear_to_db(1.0), 0.0, 1e-6);
}

TEST(ring_buffer_spsc) {
    dsp::RingBuffer rb(64, 2);
    float in[32];
    for (int i = 0; i < 32; ++i) in[i] = float(i);
    EXPECT_EQ(rb.write(in, 16), 16u);
    float out[32] = {};
    EXPECT_EQ(rb.read(out, 16), 16u);
    for (int i = 0; i < 32; ++i) EXPECT_NEAR(out[i], float(i), 1e-6);
    EXPECT_EQ(rb.available_read(), 0u);
}

TEST(resampler_module) {
    dsp::ResamplerState st;
    float in[8];
    for (int i = 0; i < 8; ++i) in[i] = 0.5f;
    float out[16];
    int out_frames = 8;
    EXPECT_EQ(dsp::resample_interleaved(st, in, 4, out, &out_frames, 2, 1.0f, DSP_SRC_LINEAR), DSP_OK);
    EXPECT_GT(out_frames, 0);
    EXPECT_NEAR(out[0], 0.5f, 1e-5);
}

TEST(backend_create) {
    DspBackend* be = dsp_backend_create();
    if (be)
        dsp_backend_destroy(be);
}

TEST(process_silence) {
    DspEngine* eng = create_engine();
    const int frames = 64;
    std::vector<float> in(frames * 2, 0.f);
    std::vector<float> out(frames * 2, 1.f);
    EXPECT_EQ(dsp_process(eng, in.data(), out.data(), frames), DSP_OK);
    for (float s : out) EXPECT_NEAR(s, 0.f, 1e-6);
    dsp_destroy(eng);
}

TEST(process_identity) {
    DspEngine* eng = create_engine();
    dsp_eq_enable(eng, 0);
    dsp_dvc_set_gain_db(eng, 0.f);
    dsp_limiter_enable(eng, 0);
    dsp_dither_enable(eng, 0);

    const int frames = 128;
    std::vector<float> in(frames * 2);
    for (int i = 0; i < frames * 2; ++i) in[i] = (i % 17) * 0.01f;
    std::vector<float> out(frames * 2);
    EXPECT_EQ(dsp_process(eng, in.data(), out.data(), frames), DSP_OK);
    for (int i = 0; i < frames * 2; ++i)
        EXPECT_NEAR(out[i], in[i], 1e-5);
    dsp_destroy(eng);
}

int main() {
    return run_all();
}
