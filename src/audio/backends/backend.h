/**
 * Platform audio backend abstraction.
 *
 * Backends handle device open/close, exclusive/low-latency mode, and
 * the realtime audio callback. The application (or engine) supplies
 * samples via a render callback or by writing into the backend's queue.
 *
 * Enable via CMake: -DDSP_BACKEND_AAUDIO=ON (Android), etc.
 */
#ifndef DSP_AUDIO_BACKEND_H
#define DSP_AUDIO_BACKEND_H

#include "dsp_engine.h"
#include <cstdint>

namespace dsp {

struct BackendConfig {
    int32_t sample_rate   = 48000;
    int32_t channels      = 2;
    int32_t buffer_frames = 256;   // preferred; device may choose different
    bool    exclusive     = true;  // prefer exclusive / low-latency
    int32_t device_id     = 0;     // 0 = default output device
};

/**
 * Called from the audio thread to fill `out` with `frames` interleaved float32 samples.
 * Must be realtime-safe (no locks, no alloc, no syscalls if possible).
 */
using BackendRenderFn = void (*)(float* out, int32_t frames, int32_t channels, void* user);

class IAudioBackend {
public:
    virtual ~IAudioBackend() = default;

    virtual DspError open(const BackendConfig& cfg) = 0;
    virtual void close() = 0;
    virtual DspError start() = 0;
    virtual DspError stop() = 0;
    virtual bool is_running() const = 0;
    virtual const char* name() const = 0;

    /** Actual rate/channels/buffer negotiated with the device after open(). */
    virtual int32_t sample_rate() const = 0;
    virtual int32_t channels() const = 0;
    virtual int32_t buffer_frames() const = 0;

    /** Set the realtime render callback (call before start). */
    virtual void set_render_callback(BackendRenderFn fn, void* user) = 0;

    /** Optional: number of underruns observed. */
    virtual uint32_t xrun_count() const { return 0; }
};

/** Null backend — no device I/O (offline / testing). */
class NullBackend : public IAudioBackend {
public:
    DspError open(const BackendConfig& cfg) override {
        cfg_ = cfg;
        opened_ = true;
        return DSP_OK;
    }
    void close() override { opened_ = false; running_ = false; }
    DspError start() override {
        if (!opened_) return DSP_ERR_DEVICE;
        running_ = true;
        return DSP_OK;
    }
    DspError stop() override { running_ = false; return DSP_OK; }
    bool is_running() const override { return running_; }
    const char* name() const override { return "null"; }
    int32_t sample_rate() const override { return cfg_.sample_rate; }
    int32_t channels() const override { return cfg_.channels; }
    int32_t buffer_frames() const override { return cfg_.buffer_frames; }
    void set_render_callback(BackendRenderFn fn, void* user) override {
        render_ = fn;
        user_ = user;
    }
private:
    BackendConfig cfg_{};
    bool opened_ = false;
    bool running_ = false;
    BackendRenderFn render_ = nullptr;
    void* user_ = nullptr;
};

/** Factories: heap-allocated backend, or nullptr if unavailable on this platform. */
IAudioBackend* create_aaudio_backend();
IAudioBackend* create_wasapi_backend();
IAudioBackend* create_coreaudio_backend();
IAudioBackend* create_default_backend(); // best available for the platform

} // namespace dsp

#endif
