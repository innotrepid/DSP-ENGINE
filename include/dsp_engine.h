/**
 * DSP ENGINE — Public C ABI
 *
 * Highly optimized 64-bit processing engine with Direct Volume Control (DVC)
 * and multi-band parametric EQ (RBJ peaking, double-precision).
 *
 * Thread-safety: process() from audio thread; set_* from one control thread.
 */

#ifndef DSP_ENGINE_H
#define DSP_ENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#if defined(_WIN32) || defined(__CYGWIN__)
  #ifdef DSP_ENGINE_EXPORTS
    #define DSP_API __declspec(dllexport)
  #else
    #define DSP_API __declspec(dllimport)
  #endif
#else
  #define DSP_API __attribute__((visibility("default")))
#endif

typedef struct DspEngine DspEngine;

typedef struct DspConfig {
    int32_t  sample_rate;
    int32_t  channels;
    int32_t  buffer_frames;
    bool     exclusive_mode;
    bool     bit_perfect;
    int32_t  realtime_priority;
} DspConfig;

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

typedef enum DspError {
    DSP_OK                  =  0,
    DSP_ERR_INVALID_ARG     = -1,
    DSP_ERR_OUT_OF_MEMORY   = -2,
    DSP_ERR_NOT_SUPPORTED   = -3,
    DSP_ERR_DEVICE          = -4,
    DSP_ERR_ALREADY_RUNNING = -5,
    DSP_ERR_NOT_RUNNING     = -6
} DspError;

/* Max parametric EQ bands (matches Resonate studio grid). */
#define DSP_EQ_MAX_BANDS 31

DSP_API DspEngine* dsp_create(const DspConfig* config);
DSP_API void dsp_destroy(DspEngine* engine);
DSP_API DspError dsp_start(DspEngine* engine);
DSP_API DspError dsp_stop(DspEngine* engine);

DSP_API void dsp_process(
    DspEngine*   engine,
    const float* in,
    float*       out,
    int32_t      frames
);

DSP_API void dsp_process_planar(
    DspEngine*    engine,
    const float** in,
    float**       out,
    int32_t       frames
);

/* ---- Direct Volume Control ---- */
DSP_API void   dsp_set_volume(DspEngine* engine, double linear_gain);
DSP_API double dsp_get_volume(const DspEngine* engine);
DSP_API void   dsp_set_volume_db(DspEngine* engine, double db);
DSP_API void   dsp_set_volume_ramped(DspEngine* engine, double linear_gain, double ramp_ms);

/* ---- Multi-band parametric EQ (native, 64-bit path) ---- */

/** Enable / disable the EQ chain. When disabled, process is DVC-only. */
DSP_API void dsp_eq_set_enabled(DspEngine* engine, bool enabled);
DSP_API bool dsp_eq_is_enabled(const DspEngine* engine);

/** Number of bands in the active bank (typically 31). */
DSP_API int32_t dsp_eq_band_count(const DspEngine* engine);

/**
 * Set one band.
 * index: 0 .. band_count-1
 * freq_hz: center frequency
 * gain_db: ±24 dB typical
 * q: bandwidth; pass <=0 to use engine default for that frequency
 */
DSP_API void dsp_eq_set_band(
    DspEngine* engine,
    int32_t    index,
    double     freq_hz,
    double     gain_db,
    double     q
);

/**
 * Bulk set gains. centers_hz may be NULL to keep existing centers.
 * count clamped to DSP_EQ_MAX_BANDS.
 */
DSP_API void dsp_eq_set_bands(
    DspEngine*   engine,
    const double* centers_hz,
    const double* gains_db,
    int32_t       count
);

/** Read back one band (control thread). */
DSP_API void dsp_eq_get_band(
    const DspEngine* engine,
    int32_t          index,
    double*          freq_hz,
    double*          gain_db,
    double*          q
);

/** Flat response + clear filter memory. */
DSP_API void dsp_eq_reset(DspEngine* engine);

/* ---- RT / status ---- */
DSP_API bool     dsp_set_realtime_priority(DspEngine* engine, int priority);
DSP_API bool     dsp_is_running(const DspEngine* engine);
DSP_API int32_t  dsp_get_sample_rate(const DspEngine* engine);
DSP_API int32_t  dsp_get_channels(const DspEngine* engine);
DSP_API int32_t  dsp_get_buffer_frames(const DspEngine* engine);
DSP_API uint64_t dsp_get_processed_frames(const DspEngine* engine);
DSP_API uint32_t dsp_get_xrun_count(const DspEngine* engine);

DSP_API const char* dsp_last_error(void);
DSP_API const char* dsp_version(void);

#ifdef __cplusplus
}
#endif

#endif /* DSP_ENGINE_H */
