/**
 * Math helpers (SIMD-friendly where possible)
 */

#include <cmath>
#include <algorithm>

namespace dsp {

double db_to_linear(double db)
{
    if (db <= -120.0) return 0.0;
    return std::pow(10.0, db / 20.0);
}

double linear_to_db(double lin)
{
    if (lin <= 1e-10) return -200.0;
    return 20.0 * std::log10(lin);
}

/** Fast approximate tanh for soft-clipping (optional future use). */
float fast_tanh(float x)
{
    // Pade approximation
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/** Clamp utility. */
template<typename T>
T clamp(T v, T lo, T hi)
{
    return std::max(lo, std::min(v, hi));
}

// Explicit instantiations
template float  clamp<float>(float, float, float);
template double clamp<double>(double, double, double);
template int    clamp<int>(int, int, int);

} // namespace dsp
