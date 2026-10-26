/**
 * Direct Volume Control — simple linear gain with optional smoothing.
 * Most of the logic is inlined in the processor; this file provides
 * any standalone helpers if needed later.
 */

#include "dsp_engine.h"
#include <cmath>

namespace dsp {

float dvc_db_to_linear(float db) {
    return std::pow(10.0f, db / 20.0f);
}

float dvc_linear_to_db(float lin) {
    if (lin <= 0.f) return -120.f;
    return 20.0f * std::log10(lin);
}

} // namespace dsp
