#include "viper/texture_resource.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <limits>

namespace viper {
namespace {

constexpr std::size_t kTextureHeaderSize = 0x10U;
constexpr std::uint32_t kMaximumMipCount = 9U;

std::uint32_t read_u32_le(const std::span<const std::byte> bytes,
                          const std::size_t offset) noexcept
{
    return static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset])) |
           (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1U])) << 8U) |
           (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2U])) << 16U) |
           (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3U])) << 24U);
}

} // namespace

std::optional<TextureResource> parse_texture_payload(
    const std::span<const std::byte> payload)
{
    if (payload.size() < kTextureHeaderSize) return std::nullopt;

    TextureResource result{};
    result.flags = std::to_integer<std::uint8_t>(payload[0]);
    result.geometry_usage = std::to_integer<std::uint8_t>(payload[1]);
    result.wrap = std::to_integer<std::uint8_t>(payload[3]);
    result.color_key = static_cast<std::uint16_t>(read_u32_le(payload, 4U));
    result.mip_count = read_u32_le(payload, 8U);

    // Real stock TEX files use 5..9 levels (16..256 square). The upper bound
    // prevents untrusted metadata from overflowing dimensions or allocations.
    if (result.mip_count < 3U || result.mip_count > kMaximumMipCount) {
        return std::nullopt;
    }
    if (result.flags != 0U && result.flags != 1U && result.flags != 2U) {
        return std::nullopt;
    }

    const auto side = std::uint32_t{1} << (result.mip_count - 1U);
    const auto pixel_count_64 = static_cast<std::uint64_t>(side) * side;
    if (pixel_count_64 > std::numeric_limits<std::size_t>::max() / 4U) {
        return std::nullopt;
    }
    const auto pixel_count = static_cast<std::size_t>(pixel_count_64);
    const auto base_bytes_64 = pixel_count_64 * 2U;
    // The chain starts with 28 reserved zero bytes, followed by the 2x2
    // level padded to a 4x4 slot. Levels 4x4 and larger are tightly packed
    // in increasing size, with the base level last. The 1x1 color is copied
    // in the header at +0x0c rather than stored in the chain.
    constexpr std::size_t chain_prefix_bytes = 28U + 32U;
    std::uint64_t expected_size = kTextureHeaderSize + chain_prefix_bytes;
    for (std::uint32_t dimension = 4U; dimension <= side; dimension *= 2U) {
        const auto level_bytes = static_cast<std::uint64_t>(dimension) *
            dimension * 2U;
        if (level_bytes > std::numeric_limits<std::uint64_t>::max() - expected_size)
            return std::nullopt;
        expected_size += level_bytes;
    }
    if (expected_size != payload.size() || base_bytes_64 > payload.size()) {
        return std::nullopt;
    }

    result.width = side;
    result.height = side;
    result.mip_rgba.resize(result.mip_count);

    const auto decode_pixel = [&result](const std::uint16_t packed) {
        if ((result.flags & 0x02U) != 0U) {
            return std::array<std::uint8_t, 4>{
                static_cast<std::uint8_t>(((packed >> 8U) & 0x0fU) << 4U),
                static_cast<std::uint8_t>(((packed >> 4U) & 0x0fU) << 4U),
                static_cast<std::uint8_t>((packed & 0x0fU) << 4U),
                static_cast<std::uint8_t>(((packed >> 12U) & 0x0fU) << 4U)};
        }
        const auto red = static_cast<std::uint8_t>((packed >> 11U) & 0x1fU);
        const auto green = static_cast<std::uint8_t>((packed >> 5U) & 0x3fU);
        const auto blue = static_cast<std::uint8_t>(packed & 0x1fU);
        return std::array<std::uint8_t, 4>{
            static_cast<std::uint8_t>(red << 3U),
            static_cast<std::uint8_t>((green & 0x3eU) << 2U),
            static_cast<std::uint8_t>(blue << 3U),
            static_cast<std::uint8_t>(result.flags == 1U &&
                packed == result.color_key ? 0U : 255U)};
    };
    const auto decode_level = [&payload, &decode_pixel](
        const std::uint32_t dimension, const std::size_t source_offset,
        const std::uint32_t source_row_stride) {
        std::vector<std::uint8_t> rgba(static_cast<std::size_t>(dimension) *
            dimension * 4U);
        for (std::uint32_t y = 0U; y < dimension; ++y) {
            for (std::uint32_t x = 0U; x < dimension; ++x) {
                const auto source_pixel = source_offset +
                    (static_cast<std::size_t>(y) * source_row_stride + x) * 2U;
                const auto packed = static_cast<std::uint16_t>(
                    std::to_integer<std::uint8_t>(payload[source_pixel]) |
                    (static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(
                        payload[source_pixel + 1U])) << 8U));
                const auto output = (static_cast<std::size_t>(y) * dimension + x) * 4U;
                const auto color = decode_pixel(packed);
                std::copy(color.begin(), color.end(), rgba.begin() +
                    static_cast<std::ptrdiff_t>(output));
            }
        }
        return rgba;
    };

    // D3D indexes level zero as full resolution, so fill levels in descending
    // dimensions even though the file stores them from small to large.
    const auto base_offset = payload.size() - static_cast<std::size_t>(base_bytes_64);
    result.mip_rgba[0] = decode_level(side, base_offset, side);
    std::size_t source_offset = base_offset;
    for (std::uint32_t dimension = side >> 1U; dimension >= 4U; dimension >>= 1U) {
        const auto level_bytes = static_cast<std::size_t>(dimension) * dimension * 2U;
        source_offset -= level_bytes;
        const auto level_index = static_cast<std::size_t>(
            std::bit_width(side) - std::bit_width(dimension));
        result.mip_rgba[level_index] = decode_level(dimension, source_offset, dimension);
    }
    result.mip_rgba[result.mip_count - 2U] = decode_level(2U,
        kTextureHeaderSize + 28U, 4U);
    const auto one_by_one = static_cast<std::uint16_t>(read_u32_le(payload, 12U));
    result.mip_rgba.back().resize(4U);
    const auto one_by_one_color = decode_pixel(one_by_one);
    std::copy(one_by_one_color.begin(), one_by_one_color.end(),
        result.mip_rgba.back().begin());
    result.rgba = result.mip_rgba.front();

    return result;
}

std::optional<TextureResource> parse_texture_file(
    const std::span<const std::byte> file)
{
    constexpr std::size_t envelope_size = 20U;
    if (file.size() <= envelope_size) return std::nullopt;
    const auto is = [&](const std::size_t offset, const char value) {
        return std::to_integer<unsigned char>(file[offset]) ==
            static_cast<unsigned char>(value);
    };
    // Standalone files use little-endian FourCC storage: "0SER", " XET",
    // version 3, then "!IGM". The resource bundle parser handles its own
    // envelope and calls parse_texture_payload directly.
    if (!is(0U, '0') || !is(1U, 'S') || !is(2U, 'E') || !is(3U, 'R') ||
        !is(4U, ' ') || !is(5U, 'X') || !is(6U, 'E') || !is(7U, 'T') ||
        !is(8U, '\x03') || !is(16U, '!') || !is(17U, 'I') ||
        !is(18U, 'G') || !is(19U, 'M')) return std::nullopt;
    return parse_texture_payload(file.subspan(envelope_size));
}

} // namespace viper
