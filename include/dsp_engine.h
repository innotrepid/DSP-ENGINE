/**
 * DSP ENGINE — Public C ABI
 *
 * Highly optimized 64-bit processing engine with Direct Volume Control (DVC).
 * Designed for ultra-low latency, bit-perfect high-resolution output,
 * near-real-time priority and drop-free operation.
 *
 * Thread-safety: All functions that take a DspEngine* are safe to call from
 * one audio/realtime thread + one control thread simultaneously.
 * The process() callback is intended to be called from the audio callback.
 */

#ifndef DSP_ENGINE_H
#define DSP_ENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* ------------------------------------------------------------------------- */
/* Export macros                                                             */
/* ------------------------------------------------------------------------- */
#if defined(_WIN32) || defined(__CYGWIN__)
  #ifdef DSP_ENGINE_EXPORTS
    #define DSP_API __declspec(dllexport)
  #else
    #define DSP_API __declspec(dllimport)
  #endif
#else
  #define DSP_API __attribute__((visibility("default")))
#endif

/* ------------------------------------------------------------------------- */
/* Opaque handle                                                             */
/* ------------------------------------------------------------------------- */
typedef struct DspEngine DspEngine;

/* ------------------------------------------------------------------------- */
/* Configuration                                                             */
/* ------------------------------------------------------------------------- */
typedef struct DspConfig {
    int32_t  sample_rate;       /* e.g. 44100, 48000, 96000, 192000          */
    int32_t  channels;          /* 1 = mono, 2 = stereo (up to 8 supported)  */
    int32_t  buffer_frames;     /* preferred callback size (64–1024 typical) */
    bool     exclusive_mode;    /* request exclusive / low-latency backend   */
    bool     bit_perfect;       /* enable pure pass-through when possible    */
    int32_t  realtime_priority; /* 0 = normal, >0 = attempt RT priority      */
} DspConfig;

/* Default-friendly initializer */
static inline DspConfig dsp_config_default(void)
{
    DspConfig c = {
        .sample_rate       = 48000,
        .channels          = 2,
        .buffer_frames     = 256,
        .exclusive_mode    = true,
        .bit_perfect       = true,
        .realtime_priority = 1
    };
    return c;
}

/* ------------------------------------------------------------------------- */
/* Error codes                                                               */
/* ------------------------------------------------------------------------- */
typedef enum DspError {
    DSP_OK                  =  0,
    DSP_ERR_INVALID_ARG     = -1,
    DSP_ERR_OUT_OF_MEMORY   = -2,
    DSP_ERR_NOT_SUPPORTED   = -3,
    DSP_ERR_DEVICE          = -4,
    DSP_ERR_ALREADY_RUNNING = -5,
    DSP_ERR_NOT_RUNNING     = -6
} DspError;

/* ------------------------------------------------------------------------- */
/* Lifecycle                                                                 */
/* ------------------------------------------------------------------------- */

/**
 * Create a new engine instance.
 * Returns NULL on failure (check last error via dsp_last_error()).
 */
DSP_API DspEngine* dsp_create(const DspConfig* config);

/**
 * Destroy the engine and free all resources.
 * Safe to call with NULL.
 */
DSP_API void dsp_destroy(DspEngine* engine);

/**
 * Start the processing / audio backend (if any).
 */
DSP_API DspError dsp_start(DspEngine* engine);

/**
 * Stop processing.
 */
DSP_API DspError dsp_stop(DspEngine* engine);

/* ------------------------------------------------------------------------- */
/* Core processing (call from audio thread)                                  */
/* ------------------------------------------------------------------------- */

/**
 * Process interleaved audio.
 *
 * in / out may be the same buffer (in-place).
 * samples are float32 interleaved.
 * Internal processing is performed in float64 (or fixed-point 64-bit)
 * for maximum precision before final conversion.
 *
 * frames = number of frames (not samples).
 */
DSP_API void dsp_process(
    DspEngine*   engine,
    const float* in,
    float*       out,
    int32_t      frames
);

/**
 * Process planar (non-interleaved) buffers.
 * in[ch] / out[ch] point to channel data.
 */
DSP_API void dsp_process_planar(
    DspEngine*    engine,
    const float** in,
    float**       out,
    int32_t       frames
);

/* ------------------------------------------------------------------------- */
/* Direct Volume Control (DVC)                                               */
/* ------------------------------------------------------------------------- */

/**
 * Set linear gain (1.0 = unity, 0.0 = silence).
 * Applied in the high-precision (64-bit) domain.
 * Thread-safe from control thread.
 */
DSP_API void dsp_set_volume(DspEngine* engine, double linear_gain);

/**
 * Get current linear gain.
 */
DSP_API double dsp_get_volume(const DspEngine* engine);

/**
 * Set volume in dB (0.0 = unity).
 */
DSP_API void dsp_set_volume_db(DspEngine* engine, double db);

/**
 * Smooth volume ramp over the given number of milliseconds.
 * Prevents clicks / zipper noise.
 */
DSP_API void dsp_set_volume_ramped(
    DspEngine* engine,
    double     linear_gain,
    double     ramp_ms
);

/* ------------------------------------------------------------------------- */
/* Real-time priority                                                        */
/* ------------------------------------------------------------------------- */

/**
 * Attempt to raise the calling thread (or engine internal thread)
 * to near-real-time priority.
 * Returns true on success.
 */
DSP_API bool dsp_set_realtime_priority(DspEngine* engine, int priority);

/* ------------------------------------------------------------------------- */
/* Status & diagnostics                                                      */
/* ------------------------------------------------------------------------- */

DSP_API bool     dsp_is_running(const DspEngine* engine);
DSP_API int32_t  dsp_get_sample_rate(const DspEngine* engine);
DSP_API int32_t  dsp_get_channels(const DspEngine* engine);
DSP_API int32_t  dsp_get_buffer_frames(const DspEngine* engine);
DSP_API uint64_t dsp_get_processed_frames(const DspEngine* engine);
DSP_API uint32_t dsp_get_xrun_count(const DspEngine* engine);  /* underruns/overruns */

DSP_API const char* dsp_last_error(void);
DSP_API const char* dsp_version(void);

#ifdef __cplusplus
}
#endif

#endif /* DSP_ENGINE_H */
