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

} // namespace viper
