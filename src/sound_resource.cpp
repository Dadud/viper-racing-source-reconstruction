#include "viper/sound_resource.hpp"

#include <limits>

namespace viper {
namespace {

std::uint16_t read_u16_le(const std::span<const std::byte> bytes,
                          const std::size_t offset) noexcept
{
    return static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(bytes[offset]) |
        (static_cast<std::uint16_t>(
            std::to_integer<std::uint8_t>(bytes[offset + 1U])) << 8U));
}

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

std::optional<PcmSoundResourceView> parse_pcm_sfx0_resource(
    const std::span<const std::byte> payload) noexcept
{
    constexpr std::size_t header_size = 20U;
    if (payload.size() < header_size) return std::nullopt;

    const auto sample_bytes = read_u32_le(payload, 0U);
    const auto format_tag = read_u16_le(payload, 4U);
    const auto channels = read_u16_le(payload, 6U);
    const auto sample_rate = read_u32_le(payload, 8U);
    const auto average_rate = read_u32_le(payload, 12U);
    const auto block_align = read_u16_le(payload, 16U);
    const auto bits_per_sample = read_u16_le(payload, 18U);
    if (format_tag != 1U || (channels != 1U && channels != 2U) ||
        (bits_per_sample != 8U && bits_per_sample != 16U) ||
        sample_rate < 8000U || sample_rate > 192000U) {
        return std::nullopt;
    }

    const auto bytes_per_sample = static_cast<std::uint32_t>(bits_per_sample / 8U);
    const auto expected_block_align = static_cast<std::uint32_t>(channels) * bytes_per_sample;
    const auto expected_average_rate =
        static_cast<std::uint64_t>(sample_rate) * expected_block_align;
    if (expected_block_align > std::numeric_limits<std::uint16_t>::max() ||
        block_align != expected_block_align ||
        expected_average_rate > std::numeric_limits<std::uint32_t>::max() ||
        average_rate != expected_average_rate || sample_bytes == 0U ||
        sample_bytes % block_align != 0U ||
        sample_bytes > payload.size() - header_size) {
        return std::nullopt;
    }

    const auto data_size = static_cast<std::size_t>(sample_bytes);
    return PcmSoundResourceView{
        format_tag, channels, sample_rate, average_rate, block_align,
        bits_per_sample, payload.subspan(header_size, data_size),
        payload.subspan(header_size + data_size),
    };
}

} // namespace viper
