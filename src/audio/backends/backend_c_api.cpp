/**
 * C ABI wrappers around IAudioBackend — matches include/dsp_engine.h
 */

#include "audio/backends/backend.h"
#include "dsp_engine.h"
#include <cstring>
#include <new>

struct DspBackend {
    dsp::IAudioBackend* impl = nullptr;
    DspRenderCallback user_cb = nullptr;
    void* user_data = nullptr;
};

static void adapt_render(float* out, int32_t frames, int32_t /*channels*/, void* user) {
    auto* be = static_cast<DspBackend*>(user);
    if (be && be->user_cb)
        be->user_cb(be->user_data, out, frames);
    else if (out && frames > 0)
        std::memset(out, 0, static_cast<size_t>(frames) * 2 * sizeof(float));
}

extern "C" {

DSP_API DspBackend* dsp_backend_create(void) {
    auto* be = new (std::nothrow) DspBackend();
    if (!be) return nullptr;
    be->impl = dsp::create_default_backend();
    if (!be->impl) {
        delete be;
        return nullptr;
    }
    return be;
}

DSP_API void dsp_backend_destroy(DspBackend* be) {
    if (!be) return;
    if (be->impl) {
        be->impl->close();
        delete be->impl;
    }
    delete be;
}

DSP_API DspError dsp_backend_open(DspBackend* be, const DspBackendConfig* cfg) {
    if (!be || !be->impl || !cfg) return DSP_ERR_INVALID_ARG;
    dsp::BackendConfig c;
    c.sample_rate   = cfg->sample_rate;
    c.channels      = cfg->channels;
    c.buffer_frames = cfg->buffer_frames;
    c.exclusive     = cfg->exclusive != 0;
    c.device_id     = cfg->device_id;
    return be->impl->open(c);
}

DSP_API void dsp_backend_close(DspBackend* be) {
    if (be && be->impl) be->impl->close();
}

DSP_API DspError dsp_backend_start(DspBackend* be, DspRenderCallback cb, void* user) {
    if (!be || !be->impl) return DSP_ERR_INVALID_ARG;
    be->user_cb = cb;
    be->user_data = user;
    be->impl->set_render_callback(&adapt_render, be);
    return be->impl->start();
}

DSP_API DspError dsp_backend_stop(DspBackend* be) {
    if (!be || !be->impl) return DSP_ERR_INVALID_ARG;
    return be->impl->stop();
}

DSP_API int dsp_backend_is_running(const DspBackend* be) {
    return (be && be->impl && be->impl->is_running()) ? 1 : 0;
}

DSP_API int dsp_backend_xrun_count(const DspBackend* be) {
    return be && be->impl ? static_cast<int>(be->impl->xrun_count()) : 0;
}

DSP_API int dsp_backend_actual_rate(const DspBackend* be) {
    return be && be->impl ? be->impl->sample_rate() : 0;
}

DSP_API int dsp_backend_actual_channels(const DspBackend* be) {
    return be && be->impl ? be->impl->channels() : 0;
}

DSP_API int dsp_backend_actual_buffer_frames(const DspBackend* be) {
    return be && be->impl ? be->impl->buffer_frames() : 0;
}

} // extern "C"
