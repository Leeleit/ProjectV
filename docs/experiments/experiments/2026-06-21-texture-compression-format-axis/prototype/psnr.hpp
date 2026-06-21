// psnr.hpp — PSNR (Peak Signal-to-Noise Ratio) computation.
// Standard image quality metric for lossy compression evaluation.
// Reference: Sheikh, Sabir & Bovik "A Statistical Evaluation of Recent Full Reference
//            Image Quality Assessment Algorithms" IEEE TIP 2006.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace texcomp {

inline double PsnrRgb(const std::uint8_t* ref, const std::uint8_t* cmp, std::size_t pixel_count) {
    double sum_sq_err = 0.0;
    for (std::size_t i = 0; i < pixel_count; ++i) {
        std::size_t base = i * 4;
        double dr = static_cast<double>(ref[base + 0]) - static_cast<double>(cmp[base + 0]);
        double dg = static_cast<double>(ref[base + 1]) - static_cast<double>(cmp[base + 1]);
        double db = static_cast<double>(ref[base + 2]) - static_cast<double>(cmp[base + 2]);
        sum_sq_err += dr * dr + dg * dg + db * db;
    }
    double mse = sum_sq_err / (3.0 * static_cast<double>(pixel_count));
    if (mse < 1e-10) return std::numeric_limits<double>::infinity();
    return 10.0 * std::log10((255.0 * 255.0) / mse);
}

inline double PsnrLuma(const std::uint8_t* ref, const std::uint8_t* cmp, std::size_t pixel_count) {
    // ITU-R BT.709 luma weights (per Aras Pranckevičius benchmark methodology).
    constexpr double wr = 0.2126, wg = 0.7152, wb = 0.0722;
    double sum_sq_err = 0.0;
    for (std::size_t i = 0; i < pixel_count; ++i) {
        std::size_t base = i * 4;
        double lr = wr * static_cast<double>(ref[base + 0]);
        double lg = wg * static_cast<double>(ref[base + 1]);
        double lb = wb * static_cast<double>(ref[base + 2]);
        double cr = wr * static_cast<double>(cmp[base + 0]);
        double cg = wg * static_cast<double>(cmp[base + 1]);
        double cb = wb * static_cast<double>(cmp[base + 2]);
        double d = (lr + lg + lb) - (cr + cg + cb);
        sum_sq_err += d * d;
    }
    double mse = sum_sq_err / static_cast<double>(pixel_count);
    if (mse < 1e-10) return std::numeric_limits<double>::infinity();
    return 10.0 * std::log10((255.0 * 255.0) / mse);
}

}  // namespace texcomp
