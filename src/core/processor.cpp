/**
 * Core 64-bit processing pipeline
 */

#include "dsp_engine.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <atomic>
#include <new>
#include <vector>

#if !defined(_WIN32)
#include <stdlib.h>
#endif

#ifdef DSP_USE_DOUBLE
using Sample = double;
#else
using Sample = float;
#endif

static void* dsp_aligned_alloc(size_t alignment, size_t size)
{
#if defined(_WIN32)
    return _aligned_malloc(size, alignment);
#else
    void* p = nullptr;
    if (posix_memalign(&p, alignment, size) != 0)
        return nullptr;
    return p;
#endif
}

static void dsp_aligned_free(void* p)
{
#if defined(_WIN32)
    _aligned_free(p);
#else
    std::free(p);
#endif
}

struct DspEngine {
    DspConfig           config;
    std::atomic<bool>   running{false};
    std::atomic<double> volume{1.0};
    std::atomic<double> target_volume{1.0};
    double              volume_step{0.0};
    int32_t             ramp_remaining{0};

    uint64_t            processed_frames{0};
    std::atomic<uint32_t> xruns{0};

    Sample*             temp_l{nullptr};
    Sample*             temp_r{nullptr};
    size_t              temp_capacity{0};

    static thread_local char last_error[256];
};

thread_local char DspEngine::last_error[256] = {0};

static void set_error(const char* msg)
{
    if (msg)
        std::strncpy(DspEngine::last_error, msg, sizeof(DspEngine::last_error) - 1);
}

extern "C" {

DSP_API DspEngine* dsp_create(const DspConfig* config)
{
    if (!config || config->channels < 1 || config->channels > 8 ||
        config->sample_rate < 8000 || config->buffer_frames < 16) {
        set_error("Invalid configuration");
        return nullptr;
    }

    auto* eng = new (std::nothrow) DspEngine();
    if (!eng) {
        set_error("Out of memory");
        return nullptr;
    }

    eng->config = *config;

    const size_t cap = static_cast<size_t>(config->buffer_frames) * 4;
    eng->temp_l = static_cast<Sample*>(dsp_aligned_alloc(64, cap * sizeof(Sample)));
    eng->temp_r = static_cast<Sample*>(dsp_aligned_alloc(64, cap * sizeof(Sample)));
    if (!eng->temp_l || !eng->temp_r) {
        dsp_aligned_free(eng->temp_l);
        dsp_aligned_free(eng->temp_r);
        delete eng;
        set_error("Failed to allocate aligned buffers");
        return nullptr;
    }
    eng->temp_capacity = cap;

    return eng;
}

DSP_API void dsp_destroy(DspEngine* engine)
{
    if (!engine) return;
    dsp_stop(engine);
    dsp_aligned_free(engine->temp_l);
    dsp_aligned_free(engine->temp_r);
    delete engine;
}

DSP_API DspError dsp_start(DspEngine* engine)
{
    if (!engine) return DSP_ERR_INVALID_ARG;
    if (engine->running.load()) return DSP_ERR_ALREADY_RUNNING;
    engine->running.store(true);
    return DSP_OK;
}

DSP_API DspError dsp_stop(DspEngine* engine)
{
    if (!engine) return DSP_ERR_INVALID_ARG;
    engine->running.store(false);
    return DSP_OK;
}

static inline void apply_dvc(DspEngine* eng, Sample* L, Sample* R, int32_t frames)
{
    double vol = eng->volume.load(std::memory_order_relaxed);

    if (eng->ramp_remaining > 0) {
        for (int32_t i = 0; i < frames; ++i) {
            L[i] = static_cast<Sample>(L[i] * vol);
            if (R) R[i] = static_cast<Sample>(R[i] * vol);

            vol += eng->volume_step;
            --eng->ramp_remaining;
            if (eng->ramp_remaining <= 0) {
                vol = eng->target_volume.load(std::memory_order_relaxed);
                eng->volume.store(vol, std::memory_order_relaxed);
                break;
            }
        }
        eng->volume.store(vol, std::memory_order_relaxed);
        return;
    }

    if (vol == 1.0) return;

    const Sample g = static_cast<Sample>(vol);
    for (int32_t i = 0; i < frames; ++i) {
        L[i] *= g;
        if (R) R[i] *= g;
    }
}

DSP_API void dsp_process(DspEngine* engine, const float* in, float* out, int32_t frames)
{
    if (!engine || !in || !out || frames <= 0) return;

    const int ch = engine->config.channels;
    const bool stereo = (ch >= 2);

    if (static_cast<size_t>(frames) > engine->temp_capacity) {
        engine->xruns.fetch_add(1);
        return;
    }

    Sample* L = engine->temp_l;
    Sample* R = stereo ? engine->temp_r : nullptr;

    if (stereo) {
        for (int32_t i = 0; i < frames; ++i) {
            L[i] = static_cast<Sample>(in[i * 2]);
            R[i] = static_cast<Sample>(in[i * 2 + 1]);
        }
    } else {
        for (int32_t i = 0; i < frames; ++i)
            L[i] = static_cast<Sample>(in[i]);
    }

    apply_dvc(engine, L, R, frames);

    if (stereo) {
        for (int32_t i = 0; i < frames; ++i) {
            out[i * 2]     = static_cast<float>(L[i]);
            out[i * 2 + 1] = static_cast<float>(R[i]);
        }
    } else {
        for (int32_t i = 0; i < frames; ++i)
            out[i] = static_cast<float>(L[i]);
    }

    engine->processed_frames += static_cast<uint64_t>(frames);
}

DSP_API void dsp_process_planar(
    DspEngine* engine,
    const float** in,
    float** out,
    int32_t frames)
{
    if (!engine || !in || !out || frames <= 0) return;

    const int ch = engine->config.channels;
    std::vector<float> tmp_in(static_cast<size_t>(frames) * ch);
    std::vector<float> tmp_out(static_cast<size_t>(frames) * ch);

    for (int32_t i = 0; i < frames; ++i)
        for (int c = 0; c < ch; ++c)
            tmp_in[static_cast<size_t>(i) * ch + c] = in[c][i];

    dsp_process(engine, tmp_in.data(), tmp_out.data(), frames);

    for (int32_t i = 0; i < frames; ++i)
        for (int c = 0; c < ch; ++c)
            out[c][i] = tmp_out[static_cast<size_t>(i) * ch + c];
}

DSP_API void dsp_set_volume(DspEngine* engine, double linear_gain)
{
    if (!engine) return;
    if (linear_gain < 0.0) linear_gain = 0.0;
    engine->volume.store(linear_gain, std::memory_order_relaxed);
    engine->target_volume.store(linear_gain, std::memory_order_relaxed);
    engine->ramp_remaining = 0;
}

DSP_API double dsp_get_volume(const DspEngine* engine)
{
    return engine ? engine->volume.load(std::memory_order_relaxed) : 0.0;
}

DSP_API void dsp_set_volume_db(DspEngine* engine, double db)
{
    if (!engine) return;
    double lin = (db <= -120.0) ? 0.0 : std::pow(10.0, db / 20.0);
    dsp_set_volume(engine, lin);
}

DSP_API void dsp_set_volume_ramped(DspEngine* engine, double linear_gain, double ramp_ms)
{
    if (!engine) return;
    if (linear_gain < 0.0) linear_gain = 0.0;

    const double current = engine->volume.load(std::memory_order_relaxed);
    const int32_t frames = static_cast<int32_t>(
        (ramp_ms / 1000.0) * engine->config.sample_rate);

    if (frames <= 0) {
        dsp_set_volume(engine, linear_gain);
        return;
    }

    engine->target_volume.store(linear_gain, std::memory_order_relaxed);
    engine->volume_step = (linear_gain - current) / frames;
    engine->ramp_remaining = frames;
}

DSP_API bool     dsp_is_running(const DspEngine* e) { return e && e->running.load(); }
DSP_API int32_t  dsp_get_sample_rate(const DspEngine* e) { return e ? e->config.sample_rate : 0; }
DSP_API int32_t  dsp_get_channels(const DspEngine* e) { return e ? e->config.channels : 0; }
DSP_API int32_t  dsp_get_buffer_frames(const DspEngine* e) { return e ? e->config.buffer_frames : 0; }
DSP_API uint64_t dsp_get_processed_frames(const DspEngine* e) { return e ? e->processed_frames : 0; }
DSP_API uint32_t dsp_get_xrun_count(const DspEngine* e) { return e ? e->xruns.load() : 0; }

DSP_API const char* dsp_last_error(void) { return DspEngine::last_error; }
DSP_API const char* dsp_version(void) { return "0.1.0"; }

} // extern "C"
