/**
 * DSP ENGINE — Public C ABI
 *
 * Highly optimized 64-bit processing engine with Direct Volume Control (DVC),
 * multi-band parametric EQ, true-peak limiter, dither, sample-rate conversion,
 * and optional platform audio backends (AAudio / WASAPI / Core Audio / ALSA).
 *
 * Version 0.3.2
 */

#ifndef DSP_ENGINE_H
#define DSP_ENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) || defined(__CYGWIN__)
  #if defined(DSP_ENGINE_STATIC)
    #define DSP_API
  #elif defined(DSP_ENGINE_EXPORTS)
    #define DSP_API __declspec(dllexport)
  #else
    #define DSP_API __declspec(dllimport)
  #endif
#else
  #define DSP_API __attribute__((visibility("default")))
#endif

typedef enum {
    DSP_OK = 0,
    DSP_ERR_INVALID_ARG = -1,
    DSP_ERR_NO_MEM = -2,
    DSP_ERR_DEVICE = -3,
    DSP_ERR_ALREADY_RUNNING = -4,
    DSP_ERR_NOT_RUNNING = -5,
    DSP_ERR_UNSUPPORTED = -6,
    DSP_ERR_OVERFLOW = -7,
    DSP_ERR_UNDERFLOW = -8
} DspError;

typedef enum {
    DSP_SRC_NONE = 0,
    DSP_SRC_LINEAR = 1,
    DSP_SRC_CUBIC = 2,
    DSP_SRC_HQ = 3
} DspSrcQuality;

typedef enum {
    DSP_DITHER_NONE = 0,
    DSP_DITHER_TPDF = 1,
    DSP_DITHER_SHAPED = 2
} DspDitherMode;

typedef struct DspEngine DspEngine;

typedef struct {
    float sample_rate;
    int channels;
    int max_block;
    int enable_eq;
    int enable_dvc;
    int enable_limiter;
    int enable_dither;
    DspDitherMode dither_mode;
    float limiter_threshold_db;
    float limiter_release_ms;
} DspConfig;

typedef struct {
    float freq_hz;
    float gain_db;
    float q;
    int type;
} DspEqBand;

DSP_API const char* dsp_version(void);
DSP_API DspEngine* dsp_create(const DspConfig* cfg);
DSP_API void       dsp_destroy(DspEngine* eng);
DSP_API DspError   dsp_reset(DspEngine* eng);
DSP_API DspError   dsp_set_sample_rate(DspEngine* eng, float rate);
DSP_API DspError   dsp_set_channels(DspEngine* eng, int channels);

DSP_API DspError dsp_process(DspEngine* eng, const float* in, float* out, int frames);

/**
 * Sample-rate conversion followed by the full DSP pipeline
 * (EQ → DVC → limiter → dither).
 * ratio ≈ source_rate / destination_rate (< 1 upsamples, > 1 downsamples).
 */
DSP_API DspError dsp_process_src(DspEngine* eng,
                                 const float* in, int in_frames,
                                 float* out, int* out_frames,
                                 float ratio, DspSrcQuality quality);

DSP_API DspError dsp_eq_enable(DspEngine* eng, int enable);
DSP_API DspError dsp_eq_set_bands(DspEngine* eng, const DspEqBand* bands, int count);
DSP_API DspError dsp_eq_set_band(DspEngine* eng, int index, const DspEqBand* band);
DSP_API int      dsp_eq_band_count(const DspEngine* eng);

DSP_API DspError dsp_dvc_set_gain_db(DspEngine* eng, float gain_db);
DSP_API float    dsp_dvc_get_gain_db(const DspEngine* eng);
DSP_API DspError dsp_dvc_enable(DspEngine* eng, int enable);

DSP_API DspError dsp_limiter_enable(DspEngine* eng, int enable);
DSP_API DspError dsp_limiter_set_threshold(DspEngine* eng, float threshold_db);
DSP_API DspError dsp_limiter_set_release(DspEngine* eng, float release_ms);
DSP_API float    dsp_limiter_get_threshold(const DspEngine* eng);
DSP_API float    dsp_limiter_get_release(const DspEngine* eng);
DSP_API int      dsp_limiter_is_enabled(const DspEngine* eng);

/** Dither: TPDF/SHAPED noise then quantize to 16-bit PCM levels (float domain). */
DSP_API DspError      dsp_dither_enable(DspEngine* eng, int enable);
DSP_API DspError      dsp_dither_set_mode(DspEngine* eng, DspDitherMode mode);
DSP_API DspDitherMode dsp_dither_get_mode(const DspEngine* eng);
DSP_API int           dsp_dither_is_enabled(const DspEngine* eng);

DSP_API int dsp_set_realtime_priority(DspEngine* eng, int priority);

/* Backend: AAudio / WASAPI / Core Audio / ALSA. Null device: start → UNSUPPORTED. */
typedef struct DspBackend DspBackend;
typedef struct {
    int sample_rate;
    int channels;
    int buffer_frames;
    int exclusive;
    int device_id;
} DspBackendConfig;
typedef void (*DspRenderCallback)(void* user, float* out, int frames);

DSP_API DspBackend* dsp_backend_create(void);
DSP_API void        dsp_backend_destroy(DspBackend* be);
DSP_API DspError dsp_backend_open(DspBackend* be, const DspBackendConfig* cfg);
DSP_API void     dsp_backend_close(DspBackend* be);
DSP_API DspError dsp_backend_start(DspBackend* be, DspRenderCallback cb, void* user);
DSP_API DspError dsp_backend_stop(DspBackend* be);
DSP_API int  dsp_backend_is_running(const DspBackend* be);
DSP_API int  dsp_backend_xrun_count(const DspBackend* be);
DSP_API int  dsp_backend_actual_rate(const DspBackend* be);
DSP_API int  dsp_backend_actual_channels(const DspBackend* be);
DSP_API int  dsp_backend_actual_buffer_frames(const DspBackend* be);

#ifdef __cplusplus
}
#endif

#endif /* DSP_ENGINE_H */
