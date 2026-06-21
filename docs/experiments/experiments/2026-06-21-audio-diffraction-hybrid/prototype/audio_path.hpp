#pragma once
//
// audio_path.hpp — common interface for audio propagation result
//
// Strategy enum + result struct shared across A_None / B_Schissler / C_Tsingos.
//

#include <array>
#include <cstdint>

namespace audio_diffraction {

enum class Strategy : std::uint8_t {
    A_None = 0,            // Phase 1 baseline (1 ray occlusion)
    B_Schissler = 1,       // Schissler 2014 UTD edge-probe (4-8 edge probes + Fresnel integral)
    C_Tsingos = 2,         // Tsingos 2007 depth-mip uniform sample (32 samples)
};

[[nodiscard]] constexpr const char* strategy_name(Strategy s) noexcept {
    switch (s) {
    case Strategy::A_None:
        return "A_None (Phase 1, 1 ray occlusion)";
    case Strategy::B_Schissler:
        return "B_Schissler (4-8 edge probes, UTD)";
    case Strategy::C_Tsingos:
        return "C_Tsingos (32 depth-mip samples)";
    }
    return "unknown";
}

struct AudioResult {
    double attenuation_db{};   // 0 = no attenuation, -20 = 20 dB muffling
    double latency_ms{};       // wall-clock latency for this source
    int probe_count{};         // number of visibility tests performed
};

}  // namespace audio_diffraction
