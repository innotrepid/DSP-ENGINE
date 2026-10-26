/**
 * C ABI wrappers around IAudioBackend.
 */

#include "audio/backends/backend.h"
#include "dsp_engine.h"
#include <atomic>
#include <cstring>
#include <new>
#include <vector>

struct DspBackend {
    dsp::IAudioBackend* impl = nullptr;
    DspEngine* engine = nullptr;
    // Optional input ring for engine mode (app writes PCM here)
    std::vector<float> input_buf;
    std::atomic<size_t> input_write{0};
    std::atomic<size_t> input_read{0};
    int32_t channels = 2;
};

// Realtime-safe engine render: process silence (or caller should set their own callback
// that feeds decoded PCM). No heap allocations.
static void engine_render_cb(float* out, int32_t frames, int32_t channels, void* user)
{
    auto* be = static_cast<DspBackend*>(user);
    if (!be || !be->engine || frames <= 0 || channels <= 0) {
        if (out && frames > 0 && channels > 0)
            std::memset(out, 0, static_cast<size_t>(frames) * channels * sizeof(float));
        return;
    }

    // Stack buffer for modest block sizes (typical AAudio burst is 96–480 frames)
    constexpr int kMaxStackFrames = 512;
    constexpr int kMaxCh = 8;
    float stack_in[kMaxStackFrames * kMaxCh];
    const int ch = channels > kMaxCh ? kMaxCh : channels;

    if (frames <= kMaxStackFrames) {
        std::memset(stack_in, 0, static_cast<size_t>(frames) * ch * sizeof(float));
        dsp_process(be->engine, stack_in, out, frames);
    } else {
        // Large callback: process in chunks
        int32_t done = 0;
        while (done < frames) {
            int32_t n = frames - done;
            if (n > kMaxStackFrames) n = kMaxStackFrames;
            std::memset(stack_in, 0, static_cast<size_t>(n) * ch * sizeof(float));
            dsp_process(be->engine, stack_in, out + done * ch, n);
            done += n;
        }
    }
}

extern "C" {

DSP_API DspBackend* dsp_backend_create(void)
{
    auto* be = new (std::nothrow) DspBackend();
    if (!be) return nullptr;
    be->impl = dsp::create_default_backend();
    if (!be->impl) {
        delete be;
        return nullptr;
    }
    return be;
}

DSP_API void dsp_backend_destroy(DspBackend* backend)
{
    if (!backend) return;
    if (backend->impl) {
        backend->impl->close();
        delete backend->impl;
    }
    delete backend;
}

DSP_API DspError dsp_backend_open(
    DspBackend* backend,
    int32_t sample_rate,
    int32_t channels,
    int32_t buffer_frames,
    bool exclusive)
{
    if (!backend || !backend->impl) return DSP_ERR_INVALID_ARG;
    dsp::BackendConfig cfg;
    cfg.sample_rate = sample_rate;
    cfg.channels = channels;
    cfg.buffer_frames = buffer_frames;
    cfg.exclusive = exclusive;
    backend->channels = channels;
    return backend->impl->open(cfg);
}

DSP_API void dsp_backend_close(DspBackend* backend)
{
    if (backend && backend->impl) backend->impl->close();
}

DSP_API DspError dsp_backend_start(DspBackend* backend)
{
    if (!backend || !backend->impl) return DSP_ERR_INVALID_ARG;
    return backend->impl->start();
}

DSP_API DspError dsp_backend_stop(DspBackend* backend)
{
    if (!backend || !backend->impl) return DSP_ERR_INVALID_ARG;
    return backend->impl->stop();
}

DSP_API bool dsp_backend_is_running(const DspBackend* backend)
{
    return backend && backend->impl && backend->impl->is_running();
}

DSP_API const char* dsp_backend_name(const DspBackend* backend)
{
    if (!backend || !backend->impl) return "none";
    return backend->impl->name();
}

DSP_API int32_t dsp_backend_sample_rate(const DspBackend* backend)
{
    return backend && backend->impl ? backend->impl->sample_rate() : 0;
}

DSP_API int32_t dsp_backend_channels(const DspBackend* backend)
{
    return backend && backend->impl ? backend->impl->channels() : 0;
}

DSP_API int32_t dsp_backend_buffer_frames(const DspBackend* backend)
{
    return backend && backend->impl ? backend->impl->buffer_frames() : 0;
}

DSP_API uint32_t dsp_backend_xrun_count(const DspBackend* backend)
{
    return backend && backend->impl ? backend->impl->xrun_count() : 0;
}

DSP_API void dsp_backend_set_render_callback(
    DspBackend* backend,
    DspBackendRenderFn fn,
    void* user)
{
    if (!backend || !backend->impl) return;
    backend->impl->set_render_callback(fn, user);
}

DSP_API void dsp_backend_set_engine(DspBackend* backend, DspEngine* engine)
{
    if (!backend || !backend->impl) return;
    backend->engine = engine;
    backend->impl->set_render_callback(&engine_render_cb, backend);
}

} // extern "C"
