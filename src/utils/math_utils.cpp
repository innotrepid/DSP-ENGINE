/**
 * Math helpers (SIMD-friendly where possible)
 */

#include <cmath>

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

} // namespace dsp
