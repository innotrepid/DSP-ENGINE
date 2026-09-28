/**
 * Direct Volume Control (DVC) — implementation details
 *
 * Currently the core logic lives inside processor.cpp for performance
 * (to keep everything in one translation unit and allow inlining).
 * This file is reserved for more advanced DVC features:
 *   - True-peak detection
 *   - Noise-shaped dither when converting to integer formats
 *   - Look-ahead gain riding
 *   - Multi-channel independent / linked volume
 */

#include "dsp_engine.h"

// Placeholder – real advanced DVC algorithms will go here.
