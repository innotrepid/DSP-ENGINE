/**
 * DSP ENGINE — Core processor
 *
 * Pipeline: EQ (MultiBandEq, typed RBJ) → DVC → true-peak limiter → dither
 * Optional SRC via dsp_process_src (linear / cubic / windowed-sinc HQ) then full pipeline.
 * Real-time safe after construction (no allocations on the process path).
 */

#include "dsp_engine.h"
#include "core/eq_biquad.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef DSP_USE_DOUBLE
using sample_t = double;
#else
using sample_t = float;
#endif

namespace {

constexpr int kMaxChannels = 8;
constexpr int kMaxEqBands  = 16;
constexpr int kMaxBlock    = 8192;
constexpr int kSincHalf    = 16;

inline sample_t db_to_lin(float db) {
    return static_cast<sample_t>(std::pow(10.0, static_cast<double>(db) / 20.0));
}

inline float sinc_weight(float x) {
    if (x == 0.f) return 1.f;
    const float pix = static_cast<float>(M_PI) * x;
    float s = std::sin(pix) / pix;
    if (x > -kSincHalf && x < kSincHalf) {
        float w = 0.5f + 0.5f * std::cos(static_cast<float>(M_PI) * x / kSincHalf);
        s *= w;
    } else {
        s = 0.f;
    }
    return s;
}

} // anonymous

struct DspEngine {
    float sample_rate = 48000.f;
    int   channels    = 2;
    int   max_block   = 1024;

    bool eq_enabled      = false;
    bool dvc_enabled     = true;
    bool limiter_enabled = false;
    bool dither_enabled  = false;

    DspDitherMode dither_mode = DSP_DITHER_TPDF;
    float limiter_threshold_db = -1.0f;
    float limiter_release_ms   = 50.f;

    dsp::MultiBandEq eq;
    int eq_band_count = 0;
    DspEqBand eq_bands[kMaxEqBands]{};

    float    dvc_gain_db  = 0.f;
    sample_t dvc_gain_lin = sample_t(1);

    sample_t lim_env[kMaxChannels]{};
    sample_t lim_gain[kMaxChannels]{};
    sample_t lim_attack  = sample_t(0.001);
    sample_t lim_release = sample_t(0.05);
    sample_t lim_thresh  = sample_t(0.89);

    uint32_t dither_state[kMaxChannels]{};

    alignas(32) sample_t temp[kMaxChannels][kMaxBlock]{};
    alignas(32) sample_t work[kMaxBlock * kMaxChannels]{};

    double* eq_ptrs[kMaxChannels]{};

    float src_phase = 0.f;

    DspEngine() {
        for (int c = 0; c < kMaxChannels; ++c) {
            lim_gain[c] = sample_t(1);
            lim_env[c]  = sample_t(0);
            dither_state[c] = 0x12345678u ^ static_cast<uint32_t>(c * 0x9E3779B9u);
            eq_ptrs[c] = reinterpret_cast<double*>(temp[c]);
        }
    }

    void update_limiter_coeffs() {
        lim_attack  = static_cast<sample_t>(1.0 - std::exp(-1.0 / (0.001 * sample_rate)));
        lim_release = static_cast<sample_t>(1.0 - std::exp(-1.0 / ((limiter_release_ms * 0.001) * sample_rate)));
        lim_thresh  = db_to_lin(limiter_threshold_db);
    }

    void rebuild_eq() {
        eq.init(sample_rate, channels);
        eq.set_band_count(eq_band_count);
        for (int i = 0; i < eq_band_count && i < kMaxEqBands; ++i) {
            const DspEqBand& b = eq_bands[i];
            eq.set_band(i, b.freq_hz, b.gain_db, b.q > 0.f ? b.q : 1.0, b.type);
        }
        eq.set_enabled(eq_enabled);
    }
};

extern "C" {

const char* dsp_version(void) {
    return "0.3.2";
}

DspEngine* dsp_create(const DspConfig* cfg) {
    if (!cfg || cfg->channels < 1 || cfg->channels > kMaxChannels ||
        cfg->max_block < 16 || cfg->max_block > kMaxBlock) {
        return nullptr;
    }
    auto* eng = new (std::nothrow) DspEngine();
    if (!eng) return nullptr;

    eng->sample_rate = cfg->sample_rate > 0 ? cfg->sample_rate : 48000.f;
    eng->channels    = cfg->channels;
    eng->max_block   = cfg->max_block;
    eng->eq_enabled      = cfg->enable_eq != 0;
    eng->dvc_enabled     = cfg->enable_dvc != 0;
    eng->limiter_enabled = cfg->enable_limiter != 0;
    eng->dither_enabled  = cfg->enable_dither != 0;
    eng->dither_mode     = cfg->dither_mode;
    eng->limiter_threshold_db = cfg->limiter_threshold_db;
    eng->limiter_release_ms   = cfg->limiter_release_ms > 0 ? cfg->limiter_release_ms : 50.f;

    eng->update_limiter_coeffs();
    eng->eq.init(eng->sample_rate, eng->channels);
    eng->eq.set_enabled(eng->eq_enabled);
    return eng;
}

void dsp_destroy(DspEngine* eng) {
    delete eng;
}

DspError dsp_reset(DspEngine* eng) {
    if (!eng) return DSP_ERR_INVALID_ARG;
    eng->eq.reset_state();
    for (int c = 0; c < eng->channels; ++c) {
        eng->lim_env[c]  = sample_t(0);
        eng->lim_gain[c] = sample_t(1);
    }
    eng->src_phase = 0.f;
    return DSP_OK;
}

DspError dsp_set_sample_rate(DspEngine* eng, float rate) {
    if (!eng || rate < 8000.f || rate > 384000.f) return DSP_ERR_INVALID_ARG;
    eng->sample_rate = rate;
    eng->update_limiter_coeffs();
    eng->rebuild_eq();
    return DSP_OK;
}

DspError dsp_set_channels(DspEngine* eng, int channels) {
    if (!eng || channels < 1 || channels > kMaxChannels) return DSP_ERR_INVALID_ARG;
    eng->channels = channels;
    eng->rebuild_eq();
    return DSP_OK;
}

static sample_t true_peak_pair(sample_t a, sample_t b) {
    sample_t peak = a >= 0 ? a : -a;
    sample_t ab = b >= 0 ? b : -b;
    if (ab > peak) peak = ab;
    sample_t m = sample_t(0.5) * (a + b);
    sample_t am = m >= 0 ? m : -m;
    if (am > peak) peak = am;
    sample_t q1 = a * sample_t(0.75) + b * sample_t(0.25);
    sample_t q3 = a * sample_t(0.25) + b * sample_t(0.75);
    sample_t aq1 = q1 >= 0 ? q1 : -q1;
    sample_t aq3 = q3 >= 0 ? q3 : -q3;
    if (aq1 > peak) peak = aq1;
    if (aq3 > peak) peak = aq3;
    return peak;
}

static void process_block(DspEngine* eng, sample_t* interleaved, int frames) {
    const int ch = eng->channels;

    for (int i = 0; i < frames; ++i)
        for (int c = 0; c < ch; ++c)
            eng->temp[c][i] = interleaved[i * ch + c];

#ifdef DSP_USE_DOUBLE
    if (eng->eq_enabled) {
        for (int c = 0; c < ch; ++c)
            eng->eq_ptrs[c] = eng->temp[c];
        eng->eq.process(eng->eq_ptrs, frames);
    }
#else
    if (eng->eq_enabled) {
        static thread_local double dbuf[kMaxChannels][kMaxBlock];
        double* ptrs[kMaxChannels];
        for (int c = 0; c < ch; ++c) {
            for (int i = 0; i < frames; ++i)
                dbuf[c][i] = static_cast<double>(eng->temp[c][i]);
            ptrs[c] = dbuf[c];
        }
        eng->eq.process(ptrs, frames);
        for (int c = 0; c < ch; ++c)
            for (int i = 0; i < frames; ++i)
                eng->temp[c][i] = static_cast<sample_t>(dbuf[c][i]);
    }
#endif

    if (eng->dvc_enabled && eng->dvc_gain_lin != sample_t(1)) {
        const sample_t g = eng->dvc_gain_lin;
        for (int c = 0; c < ch; ++c)
            for (int i = 0; i < frames; ++i)
                eng->temp[c][i] *= g;
    }

    if (eng->limiter_enabled) {
        const sample_t thr = eng->lim_thresh;
        const sample_t atk = eng->lim_attack;
        const sample_t rel = eng->lim_release;
        for (int i = 0; i < frames; ++i) {
            sample_t peak = sample_t(0);
            for (int c = 0; c < ch; ++c) {
                sample_t x = eng->temp[c][i];
                sample_t ax = x >= 0 ? x : -x;
                if (ax > peak) peak = ax;
                if (i + 1 < frames) {
                    sample_t tp = true_peak_pair(eng->temp[c][i], eng->temp[c][i + 1]);
                    if (tp > peak) peak = tp;
                }
            }
            sample_t env = eng->lim_env[0];
            sample_t gain = eng->lim_gain[0];
            if (peak > env)
                env += atk * (peak - env);
            else
                env += rel * (peak - env);

            sample_t target = (env > thr) ? (thr / (env + sample_t(1e-12))) : sample_t(1);
            if (target < gain)
                gain += atk * (target - gain);
            else
                gain += rel * (target - gain);

            eng->lim_env[0]  = env;
            eng->lim_gain[0] = gain;
            for (int c = 0; c < ch; ++c)
                eng->temp[c][i] *= gain;
        }
        for (int c = 1; c < ch; ++c) {
            eng->lim_env[c]  = eng->lim_env[0];
            eng->lim_gain[c] = eng->lim_gain[0];
        }
    }

    if (eng->dither_enabled && eng->dither_mode != DSP_DITHER_NONE) {
        /* 16-bit quantize with TPDF (or light error feedback for SHAPED). */
        constexpr float kScale = 32768.f;
        constexpr float kInv   = 1.f / 32768.f;
        for (int c = 0; c < ch; ++c) {
            uint32_t s = eng->dither_state[c];
            float err = 0.f;
            for (int i = 0; i < frames; ++i) {
                s ^= s << 13; s ^= s >> 17; s ^= s << 5;
                float r1 = (s & 0xFFFFFF) / float(0x1000000) - 0.5f;
                s ^= s << 13; s ^= s >> 17; s ^= s << 5;
                float r2 = (s & 0xFFFFFF) / float(0x1000000) - 0.5f;
                float noise = r1 + r2;

                float x = static_cast<float>(eng->temp[c][i]);
                if (eng->dither_mode == DSP_DITHER_SHAPED)
                    x += err;

                float scaled = x * kScale + noise;
                float q = std::floor(scaled + 0.5f);
                if (q >  32767.f) q =  32767.f;
                if (q < -32768.f) q = -32768.f;

                if (eng->dither_mode == DSP_DITHER_SHAPED)
                    err = (scaled - q) * 0.5f;

                eng->temp[c][i] = static_cast<sample_t>(q * kInv);
            }
            eng->dither_state[c] = s;
        }
    }

    for (int i = 0; i < frames; ++i)
        for (int c = 0; c < ch; ++c)
            interleaved[i * ch + c] = eng->temp[c][i];
}

DspError dsp_process(DspEngine* eng, const float* in, float* out, int frames) {
    if (!eng || !in || !out || frames < 1 || frames > eng->max_block)
        return DSP_ERR_INVALID_ARG;

    const int n = frames * eng->channels;
    for (int i = 0; i < n; ++i)
        eng->work[i] = static_cast<sample_t>(in[i]);

    process_block(eng, eng->work, frames);

    for (int i = 0; i < n; ++i)
        out[i] = static_cast<float>(eng->work[i]);
    return DSP_OK;
}

DspError dsp_process_src(DspEngine* eng,
                         const float* in, int in_frames,
                         float* out, int* out_frames,
                         float ratio, DspSrcQuality quality)
{
    if (!eng || !in || !out || !out_frames || in_frames < 1 || ratio <= 0.f)
        return DSP_ERR_INVALID_ARG;

    const int ch = eng->channels;
    int max_out = *out_frames;
    if (max_out < 1) return DSP_ERR_INVALID_ARG;

    float phase = eng->src_phase;
    int written = 0;

    auto sample_at = [&](int idx, int c) -> float {
        if (idx < 0 || idx >= in_frames) return 0.f;
        return in[idx * ch + c];
    };

    while (written < max_out) {
        int i0 = static_cast<int>(phase);
        float frac = phase - static_cast<float>(i0);
        if (i0 >= in_frames - 1 && quality != DSP_SRC_HQ) break;
        if (i0 >= in_frames) break;

        for (int c = 0; c < ch; ++c) {
            float y;
            if (quality == DSP_SRC_HQ) {
                float sum = 0.f;
                float wsum = 0.f;
                for (int k = -kSincHalf; k <= kSincHalf; ++k) {
                    float w = sinc_weight(static_cast<float>(k) - frac);
                    sum  += w * sample_at(i0 + k, c);
                    wsum += w;
                }
                y = (wsum != 0.f) ? (sum / wsum) : sample_at(i0, c);
            } else if (quality == DSP_SRC_CUBIC) {
                float y0 = sample_at(i0 - 1, c);
                float y1 = sample_at(i0,     c);
                float y2 = sample_at(i0 + 1, c);
                float y3 = sample_at(i0 + 2, c);
                float a0 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
                float a1 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3;
                float a2 = -0.5f * y0 + 0.5f * y2;
                float a3 = y1;
                y = ((a0 * frac + a1) * frac + a2) * frac + a3;
            } else if (quality == DSP_SRC_LINEAR) {
                float y1 = sample_at(i0,     c);
                float y2 = sample_at(i0 + 1, c);
                y = y1 + frac * (y2 - y1);
            } else {
                y = sample_at(i0, c);
            }
            out[written * ch + c] = y;
        }
        ++written;
        phase += ratio;
    }

    if (phase >= static_cast<float>(in_frames))
        eng->src_phase = 0.f;
    else
        eng->src_phase = phase - static_cast<float>(static_cast<int>(phase));

    /* Full DSP pipeline on resampled output (chunked to max_block). */
    int done = 0;
    while (done < written) {
        int n = written - done;
        if (n > eng->max_block) n = eng->max_block;
        const int samples = n * ch;
        for (int i = 0; i < samples; ++i)
            eng->work[i] = static_cast<sample_t>(out[done * ch + i]);
        process_block(eng, eng->work, n);
        for (int i = 0; i < samples; ++i)
            out[done * ch + i] = static_cast<float>(eng->work[i]);
        done += n;
    }

    *out_frames = written;
    return DSP_OK;
}

DspError dsp_eq_enable(DspEngine* eng, int enable) {
    if (!eng) return DSP_ERR_INVALID_ARG;
    eng->eq_enabled = enable != 0;
    eng->eq.set_enabled(eng->eq_enabled);
    return DSP_OK;
}

DspError dsp_eq_set_bands(DspEngine* eng, const DspEqBand* bands, int count) {
    if (!eng || (!bands && count > 0) || count < 0 || count > kMaxEqBands)
        return DSP_ERR_INVALID_ARG;
    eng->eq_band_count = count;
    if (count > 0)
        std::memcpy(eng->eq_bands, bands, static_cast<size_t>(count) * sizeof(DspEqBand));
    eng->rebuild_eq();
    return DSP_OK;
}

DspError dsp_eq_set_band(DspEngine* eng, int index, const DspEqBand* band) {
    if (!eng || !band || index < 0 || index >= kMaxEqBands)
        return DSP_ERR_INVALID_ARG;
    if (index >= eng->eq_band_count)
        eng->eq_band_count = index + 1;
    eng->eq_bands[index] = *band;
    eng->rebuild_eq();
    return DSP_OK;
}

int dsp_eq_band_count(const DspEngine* eng) {
    return eng ? eng->eq_band_count : 0;
}

DspError dsp_dvc_set_gain_db(DspEngine* eng, float gain_db) {
    if (!eng) return DSP_ERR_INVALID_ARG;
    eng->dvc_gain_db  = gain_db;
    eng->dvc_gain_lin = db_to_lin(gain_db);
    return DSP_OK;
}

float dsp_dvc_get_gain_db(const DspEngine* eng) {
    return eng ? eng->dvc_gain_db : 0.f;
}

DspError dsp_dvc_enable(DspEngine* eng, int enable) {
    if (!eng) return DSP_ERR_INVALID_ARG;
    eng->dvc_enabled = enable != 0;
    return DSP_OK;
}

DspError dsp_limiter_enable(DspEngine* eng, int enable) {
    if (!eng) return DSP_ERR_INVALID_ARG;
    eng->limiter_enabled = enable != 0;
    return DSP_OK;
}

DspError dsp_limiter_set_threshold(DspEngine* eng, float threshold_db) {
    if (!eng) return DSP_ERR_INVALID_ARG;
    eng->limiter_threshold_db = threshold_db;
    eng->update_limiter_coeffs();
    return DSP_OK;
}

DspError dsp_limiter_set_release(DspEngine* eng, float release_ms) {
    if (!eng || release_ms <= 0.f) return DSP_ERR_INVALID_ARG;
    eng->limiter_release_ms = release_ms;
    eng->update_limiter_coeffs();
    return DSP_OK;
}

DspError dsp_dither_enable(DspEngine* eng, int enable) {
    if (!eng) return DSP_ERR_INVALID_ARG;
    eng->dither_enabled = enable != 0;
    return DSP_OK;
}

DspError dsp_dither_set_mode(DspEngine* eng, DspDitherMode mode) {
    if (!eng) return DSP_ERR_INVALID_ARG;
    eng->dither_mode = mode;
    return DSP_OK;
}

} // extern "C"
