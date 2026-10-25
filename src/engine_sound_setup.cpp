#include "viper/engine_sound_setup.hpp"

#include <bit>
#include <cmath>

namespace viper {
namespace {

std::uint32_t read_u32_le(const std::span<const std::byte> bytes,
                          const std::size_t offset) noexcept
{
    return static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset])) |
        (static_cast<std::uint32_t>(
            std::to_integer<std::uint8_t>(bytes[offset + 1U])) << 8U) |
        (static_cast<std::uint32_t>(
            std::to_integer<std::uint8_t>(bytes[offset + 2U])) << 16U) |
        (static_cast<std::uint32_t>(
            std::to_integer<std::uint8_t>(bytes[offset + 3U])) << 24U);
}

} // namespace

std::optional<EngineSoundSetupView> parse_engine_sound_setup(
    const std::span<const std::byte> payload) noexcept
{
    constexpr std::size_t count_size = 4U;
    constexpr std::size_t words_per_band = 7U;
    constexpr std::size_t word_size = 4U;
    constexpr std::size_t max_bands = 7U;
    if (payload.size() < count_size) return std::nullopt;

    const auto count = read_u32_le(payload, 0U);
    if (count > max_bands) return std::nullopt;
    const auto record_bytes = static_cast<std::size_t>(count) *
        words_per_band * word_size;
    if (record_bytes > payload.size() - count_size) return std::nullopt;

    EngineSoundSetupView result;
    result.band_count = count;
    for (std::size_t band = 0; band < count; ++band) {
        for (std::size_t field = 0; field < words_per_band; ++field) {
            const auto offset = count_size +
                (band * words_per_band + field) * word_size;
            const auto value = std::bit_cast<float>(read_u32_le(payload, offset));
            if (!std::isfinite(value)) return std::nullopt;
            result.bands[band][field] = value;
        }
    }
    result.opaque_tail = payload.subspan(count_size + record_bytes);
    return result;
}

EngineSoundBandEvaluation evaluate_engine_sound_band_46c930(
    const EngineSoundBandInput& input) noexcept
{
    const auto invalid = [] {
        return EngineSoundBandEvaluation{
            EngineSoundBandStatus::invalid_or_nonfinite_input,
            EngineSoundBandBranch::outside_gate,
            0.0F};
    };
    for (const auto value : input.band) {
        if (!std::isfinite(value)) return invalid();
    }
    if (!std::isfinite(input.control_value) || !std::isfinite(input.outside_value))
        return invalid();

    const auto& b = input.band;
    // The first branch returns when x < b0; the second returns when x >= b3.
    // Thus b0 is inclusive and b3 is exclusive in the active interval.
    if (input.control_value < b[0] || input.control_value >= b[3]) {
        return {EngineSoundBandStatus::evaluated,
                EngineSoundBandBranch::outside_gate, input.outside_value};
    }
    if (!input.division_helper_gate_clear) {
        return {EngineSoundBandStatus::unresolved_division_helper,
                EngineSoundBandBranch::central_ramp, 0.0F};
    }

    const auto divide = [](const float numerator, const float denominator,
                           float& result) {
        if (denominator == 0.0F || !std::isfinite(denominator)) return false;
        result = numerator / denominator;
        return std::isfinite(result);
    };

    // The native FPU computes (x - b1) / (b2 - b1), then chooses an
    // additional ramp factor only for ratios above one or below zero.
    float ratio = 0.0F;
    if (!divide(input.control_value - b[1], b[2] - b[1], ratio))
        return {EngineSoundBandStatus::unresolved_division_helper,
                EngineSoundBandBranch::central_ramp, 0.0F};

    float ramp = 1.0F;
    auto branch = EngineSoundBandBranch::central_ramp;
    if (ratio > 1.0F) {
        branch = EngineSoundBandBranch::upper_ramp;
        if (!divide(input.control_value - b[3], b[2] - b[3], ramp))
            return {EngineSoundBandStatus::unresolved_division_helper, branch, 0.0F};
    } else if (ratio < 0.0F) {
        branch = EngineSoundBandBranch::lower_ramp;
        if (!divide(input.control_value - b[0], b[1] - b[0], ramp))
            return {EngineSoundBandStatus::unresolved_division_helper, branch, 0.0F};
    }

    const float interpolated = (b[5] - b[4]) * ratio + b[4];
    const float result = interpolated * ramp;
    if (!std::isfinite(result)) return invalid();
    return {EngineSoundBandStatus::evaluated, branch, result};
}

EngineSoundUpdateResult apply_engine_sound_update_46ca10(
    const EngineSoundUpdateInput& input) noexcept
{
    EngineSoundUpdateResult result;
    if (!input.sound_object) {
        result.status = EngineSoundUpdateStatus::missing_sound_object;
        return result;
    }
    result.sound_object = *input.sound_object;
    if (!std::isfinite(input.object_scale) ||
        !std::isfinite(result.sound_object.value_at_04) ||
        !std::isfinite(result.sound_object.value_at_08)) {
        result.status = EngineSoundUpdateStatus::invalid_or_nonfinite_input;
        return result;
    }

    const auto band = evaluate_engine_sound_band_46c930(input.band_input);
    result.branch = band.branch;
    if (band.status == EngineSoundBandStatus::invalid_or_nonfinite_input) {
        result.status = EngineSoundUpdateStatus::invalid_or_nonfinite_input;
        return result;
    }
    if (band.status == EngineSoundBandStatus::unresolved_division_helper) {
        result.status = EngineSoundUpdateStatus::unresolved_division_helper;
        return result;
    }

    // 0x46CA10 multiplies the 0x46C930 result by arg3 and disables the
    // object for the frame when the result compares <= DAT_004C9674 (zero).
    const float scaled_band = band.value * input.object_scale;
    if (!std::isfinite(scaled_band)) {
        result.status = EngineSoundUpdateStatus::invalid_or_nonfinite_input;
        return result;
    }
    if (scaled_band <= 0.0F) {
        result.sound_object.flag_at_2a = true;
        result.status = EngineSoundUpdateStatus::sound_disabled_for_frame;
        return result;
    }

    // In the active path, native divides arg1 by ESFX field 6, scales the
    // band's result by 0.6, writes changed values to +8/+4, and sets +29/+2B
    // while clearing +2A. Division-helper behavior remains caller-gated.
    if (!input.band_input.division_helper_gate_clear ||
        input.band_input.band[6] == 0.0F) {
        result.status = EngineSoundUpdateStatus::unresolved_division_helper;
        return result;
    }
    const float target_at_04 = input.band_input.control_value /
        input.band_input.band[6];
    const float target_at_08 = scaled_band * 0.6F;
    if (!std::isfinite(target_at_04) || !std::isfinite(target_at_08)) {
        result.status = EngineSoundUpdateStatus::invalid_or_nonfinite_input;
        return result;
    }
    if (result.sound_object.value_at_08 != target_at_08) {
        result.sound_object.flag_at_2c = true;
        result.sound_object.value_at_08 = target_at_08;
    }
    if (result.sound_object.value_at_04 != target_at_04) {
        result.sound_object.flag_at_2d = true;
        result.sound_object.value_at_04 = target_at_04;
    }
    result.sound_object.flag_at_2b = true;
    result.sound_object.flag_at_29 = true;
    result.sound_object.flag_at_2a = false;
    result.status = EngineSoundUpdateStatus::updated;
    return result;
}

} // namespace viper
