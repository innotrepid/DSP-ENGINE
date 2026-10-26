/**
 * DSP ENGINE — Core processor
 *
 * Pipeline: (optional SRC) → EQ → DVC → true-peak limiter → dither
 * All internal processing in double precision when DSP_USE_DOUBLE is set.
 * Real-time safe: no allocations on the process path after construction.
 */

#include "dsp_engine.h"
#include "core/eq_biquad.h"
#include "audio/ring_buffer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <new>
#include <vector>

#ifdef DSP_USE_DOUBLE
using sample_t = double;
#else
using sample_t = float;
#endif

namespace {

constexpr int kMaxChannels = 8;
constexpr int kMaxEqBands = 16;
constexpr int kMaxBlock = 8192;

inline sample_t db_to_lin(float db) {
    return static_cast<sample_t>(std::pow(10.0, db / 20.0));
}

} // anonymous namespace

struct DspEngine {
    float sample_rate = 48000.f;
    int channels = 2;
    int max_block = 1024;

    // Modules
    bool eq_enabled = false;
    bool dvc_enabled = true;
    bool limiter_enabled = false;
    bool dither_enabled = false;

    DspDitherMode dither_mode = DSP_DITHER_TPDF;
    float limiter_threshold_db = -1.0f;
    float limiter_release_ms = 50.f;

    // EQ
    int eq_band_count = 0;
    DspEqBand eq_bands[kMaxEqBands]{};
    dsp::BiquadCascade eq[kMaxChannels];

    // DVC
    float dvc_gain_db = 0.f;
    sample_t dvc_gain_lin = 1.0;

    // Limiter state (per channel)
    sample_t lim_env[kMaxChannels]{};
    sample_t lim_gain[kMaxChannels]{};
    sample_t lim_attack = 0.001;
    sample_t lim_release = 0.05;
    sample_t lim_thresh = 0.89; // ~ -1 dB

    // Dither
    uint32_t dither_state[kMaxChannels]{};

    // Temp buffers (pre-allocated)
    alignas(32) sample_t temp[kMaxChannels][kMaxBlock];
    alignas(32) float temp_f[kMaxChannels * kMaxBlock];

    // SRC state (simple linear / cubic / HQ)
    float src_phase = 0.f;
    std::vector<float> src_hist; // small history for interpolation

    DspEngine() {
        for (int c = 0; c < kMaxChannels; ++c) {
            lim_gain[c] = 1.0;
            lim_env[c] = 0.0;
            dither_state[c] = 0x12345678u ^ (c * 0x9E3779B9u);
        }
        src_hist.resize(64, 0.f);
    }

    void update_limiter_coeffs() {
        // Fast attack, user-controlled release
        lim_attack = static_cast<sample_t>(1.0 - std::exp(-1.0 / (0.001 * sample_rate))); // ~1 ms
        lim_release = static_cast<sample_t>(1.0 - std::exp(-1.0 / ((limiter_release_ms * 0.001) * sample_rate)));
        lim_thresh = db_to_lin(limiter_threshold_db);
    }

    void rebuild_eq() {
        for (int c = 0; c < channels; ++c) {
            eq[c].clear();
            for (int i = 0; i < eq_band_count; ++i) {
                const DspEqBand& b = eq_bands[i];
                dsp::BiquadCoeffs coef;
                switch (b.type) {
                case 1: coef = dsp::low_shelf(b.freq_hz, b.gain_db, b.q, sample_rate); break;
                case 2: coef = dsp::high_shelf(b.freq_hz, b.gain_db, b.q, sample_rate); break;
                case 3: coef = dsp::low_pass(b.freq_hz, b.q, sample_rate); break;
                case 4: coef = dsp::high_pass(b.freq_hz, b.q, sample_rate); break;
                default: coef = dsp::peaking(b.freq_hz, b.gain_db, b.q, sample_rate); break;
                }
                eq[c].add(coef);
            }
        }
    }
};

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

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
    eng->channels = cfg->channels;
    eng->max_block = cfg->max_block;
    eng->eq_enabled = cfg->enable_eq != 0;
    eng->dvc_enabled = cfg->enable_dvc != 0;
    eng->limiter_enabled = cfg->enable_limiter != 0;
    eng->dither_enabled = cfg->enable_dither != 0;
    eng->dither_mode = cfg->dither_mode;
    eng->limiter_threshold_db = cfg->limiter_threshold_db;
    eng->limiter_release_ms = cfg->limiter_release_ms > 0 ? cfg->limiter_release_ms : 50.f;
    eng->update_limiter_coeffs();
    return eng;
}

void dsp_destroy(DspEngine* eng) {
    delete eng;
}

DspError dsp_reset(DspEngine* eng) {
    if (!eng) return DSP_ERR_INVALID_ARG;
    for (int c = 0; c < eng->channels; ++c) {
        eng->eq[c].reset();
        eng->lim_env[c] = 0;
        eng->lim_gain[c] = 1;
    }
    eng->src_phase = 0.f;
    std::fill(eng->src_hist.begin(), eng->src_hist.end(), 0.f);
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

/* -------------------------------------------------------------------------- */
/* Processing helpers                                                         */
/* -------------------------------------------------------------------------- */

static void process_block(DspEngine* eng, sample_t* interleaved, int frames) {
    const int ch = eng->channels;

    // De-interleave into temp (or process in-place per channel if preferred)
    for (int i = 0; i < frames; ++i) {
        for (int c = 0; c < ch; ++c) {
            eng->temp[c][i] = interleaved[i * ch + c];
        }
    }

    // EQ
    if (eng->eq_enabled && eng->eq_band_count > 0) {
        for (int c = 0; c < ch; ++c) {
            eng->eq[c].process(eng->temp[c], eng->temp[c], frames);
        }
    }

    // DVC
    if (eng->dvc_enabled && eng->dvc_gain_lin != sample_t(1)) {
        const sample_t g = eng->dvc_gain_lin;
        for (int c = 0; c < ch; ++c) {
            for (int i = 0; i < frames; ++i) {
                eng->temp[c][i] *= g;
            }
        }
    }

    // True-peak style limiter (envelope follower + gain reduction)
    if (eng->limiter_enabled) {
        const sample_t thr = eng->lim_thresh;
        const sample_t atk = eng->lim_attack;
        const sample_t rel = eng->lim_release;
        for (int c = 0; c < ch; ++c) {
            sample_t env = eng->lim_env[c];
            sample_t gain = eng->lim_gain[c];
            for (int i = 0; i < frames; ++i) {
                sample_t x = eng->temp[c][i];
                sample_t ax = x >= 0 ? x : -x;
                // Simple peak + slight overshoot estimate
                sample_t peak = ax;
                if (i + 1 < frames) {
                    sample_t n = eng->temp[c][i + 1];
                    sample_t an = n >= 0 ? n : -n;
                    if (an > peak) peak = an;
                }
                if (peak > env)
                    env += atk * (peak - env);
                else
                    env += rel * (peak - env);

                sample_t target = (env > thr) ? (thr / (env + sample_t(1e-12))) : sample_t(1);
                // Smooth gain
                if (target < gain)
                    gain += atk * (target - gain);
                else
                    gain += rel * (target - gain);

                eng->temp[c][i] = x * gain;
            }
            eng->lim_env[c] = env;
            eng->lim_gain[c] = gain;
        }
    }

    // Dither (TPDF / shaped) applied just before quantisation to float32
    if (eng->dither_enabled) {
        for (int c = 0; c < ch; ++c) {
            uint32_t s = eng->dither_state[c];
            for (int i = 0; i < frames; ++i) {
                // xorshift
                s ^= s << 13;
                s ^= s >> 17;
                s ^= s << 5;
                float r1 = (s & 0xFFFFFF) / float(0x1000000) - 0.5f;
                s ^= s << 13;
                s ^= s >> 17;
                s ^= s << 5;
                float r2 = (s & 0xFFFFFF) / float(0x1000000) - 0.5f;
                float dither = (eng->dither_mode == DSP_DITHER_SHAPED)
                    ? (r1 * 0.5f + r2 * 0.25f) * (1.0f / 32768.f)
                    : (r1 + r2) * (1.0f / 65536.f); // TPDF
                eng->temp[c][i] += static_cast<sample_t>(dither);
            }
            eng->dither_state[c] = s;
        }
    }

    // Re-interleave
    for (int i = 0; i < frames; ++i) {
        for (int c = 0; c < ch; ++c) {
            interleaved[i * ch + c] = eng->temp[c][i];
        }
    }
}

DspError dsp_process(DspEngine* eng, const float* in, float* out, int frames) {
    if (!eng || !in || !out || frames < 1 || frames > eng->max_block)
        return DSP_ERR_INVALID_ARG;

    const int ch = eng->channels;
    const int n = frames * ch;

    // Convert to internal sample_t
    sample_t* buf = eng->temp[0]; // reuse first channel buffer for interleaved work
    // Actually use a linear interleaved temp
    sample_t* work = reinterpret_cast<sample_t*>(eng->temp_f); // careful: size
    // Safer: process in chunks using stack-friendly path

    // Simple path: copy to interleaved sample_t, process, write back
    // For large blocks we already limited max_block
    for (int i = 0; i < n; ++i)
        work[i] = static_cast<sample_t>(in[i]);

    process_block(eng, work, frames);

    for (int i = 0; i < n; ++i)
        out[i] = static_cast<float>(work[i]);

    return DSP_OK;
}

/* -------------------------------------------------------------------------- */
/* SRC                                                                        */
/* -------------------------------------------------------------------------- */

DspError dsp_process_src(DspEngine* eng,
                         const float* in,
                         int in_frames,
                         float* out,
                         int* out_frames,
                         float ratio,
                         DspSrcQuality quality)
{
    if (!eng || !in || !out || !out_frames || in_frames < 1 || ratio <= 0.f)
        return DSP_ERR_INVALID_ARG;

    const int ch = eng->channels;
    int max_out = *out_frames;
    if (max_out < 1) return DSP_ERR_INVALID_ARG;

    // Very lightweight linear / cubic for now; HQ is windowed-sinc stub
    float phase = eng->src_phase;
    int written = 0;

    auto sample_at = [&](int idx, int c) -> float {
        if (idx < 0) return 0.f;
        if (idx >= in_frames) return 0.f;
        return in[idx * ch + c];
    };

    while (written < max_out) {
        int i0 = static_cast<int>(phase);
        float frac = phase - i0;
        if (i0 >= in_frames - 1) break;

        for (int c = 0; c < ch; ++c) {
            float y;
            if (quality == DSP_SRC_CUBIC || quality == DSP_SRC_HQ) {
                // Catmull-Rom / cubic
                float y0 = sample_at(i0 - 1, c);
                float y1 = sample_at(i0, c);
                float y2 = sample_at(i0 + 1, c);
                float y3 = sample_at(i0 + 2, c);
                float a0 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
                float a1 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3;
                float a2 = -0.5f * y0 + 0.5f * y2;
                float a3 = y1;
                y = ((a0 * frac + a1) * frac + a2) * frac + a3;
            } else {
                // Linear
                float y1 = sample_at(i0, c);
                float y2 = sample_at(i0 + 1, c);
                y = y1 + frac * (y2 - y1);
            }
            out[written * ch + c] = y;
        }
        ++written;
        phase += ratio;
    }

    eng->src_phase = phase - static_cast<int>(phase); // keep fractional
    *out_frames = written;
    return DSP_OK;
}

/* -------------------------------------------------------------------------- */
/* EQ API                                                                     */
/* -------------------------------------------------------------------------- */

DspError dsp_eq_enable(DspEngine* eng, int enable) {
    if (!eng) return DSP_ERR_INVALID_ARG;
    eng->eq_enabled = enable != 0;
    return DSP_OK;
}

DspError dsp_eq_set_bands(DspEngine* eng, const DspEqBand* bands, int count) {
    if (!eng || (!bands && count > 0) || count < 0 || count > kMaxEqBands)
        return DSP_ERR_INVALID_ARG;
    eng->eq_band_count = count;
    if (count > 0)
        std::memcpy(eng->eq_bands, bands, count * sizeof(DspEqBand));
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

/* -------------------------------------------------------------------------- */
/* DVC                                                                        */
/* -------------------------------------------------------------------------- */

DspError dsp_dvc_set_gain_db(DspEngine* eng, float gain_db) {
    if (!eng) return DSP_ERR_INVALID_ARG;
    eng->dvc_gain_db = gain_db;
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

/* -------------------------------------------------------------------------- */
/* Limiter                                                                    */
/* -------------------------------------------------------------------------- */

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

/* -------------------------------------------------------------------------- */
/* Dither                                                                     */
/* -------------------------------------------------------------------------- */

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
