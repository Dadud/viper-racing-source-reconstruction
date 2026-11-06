#include "viper/texture_resource.hpp"

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
    if (result.mip_count == 0U || result.mip_count > kMaximumMipCount) {
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
    if (base_bytes_64 > payload.size() - kTextureHeaderSize) {
        return std::nullopt;
    }

    result.width = side;
    result.height = side;
    result.rgba.resize(pixel_count * 4U);

    // The complete mip chain places its full-resolution level last.
    const auto base_offset = payload.size() - static_cast<std::size_t>(base_bytes_64);
    for (std::size_t pixel = 0; pixel < pixel_count; ++pixel) {
        const auto offset = base_offset + pixel * 2U;
        const auto packed = static_cast<std::uint16_t>(
            std::to_integer<std::uint8_t>(payload[offset]) |
            (static_cast<std::uint16_t>(
                std::to_integer<std::uint8_t>(payload[offset + 1U])) << 8U));

        const auto output = pixel * 4U;
        if ((result.flags & 0x02U) != 0U) {
            // Native alpha textures use 16-bit ARGB4444 in the confirmed 0x02
            // variant. The 0x03 four-byte variant is rejected above.
            result.rgba[output] = static_cast<std::uint8_t>(((packed >> 8U) & 0x0fU) << 4U);
            result.rgba[output + 1U] = static_cast<std::uint8_t>(((packed >> 4U) & 0x0fU) << 4U);
            result.rgba[output + 2U] = static_cast<std::uint8_t>((packed & 0x0fU) << 4U);
            result.rgba[output + 3U] = static_cast<std::uint8_t>(((packed >> 12U) & 0x0fU) << 4U);
            continue;
        }

        const auto red = static_cast<std::uint8_t>((packed >> 11U) & 0x1fU);
        const auto green = static_cast<std::uint8_t>((packed >> 5U) & 0x3fU);
        const auto blue = static_cast<std::uint8_t>(packed & 0x1fU);
        result.rgba[output] = static_cast<std::uint8_t>(red << 3U);
        // The low green bit is unused by the native conversion path.
        result.rgba[output + 1U] = static_cast<std::uint8_t>((green & 0x3eU) << 2U);
        result.rgba[output + 2U] = static_cast<std::uint8_t>(blue << 3U);
        result.rgba[output + 3U] = (result.flags == 1U && packed == result.color_key)
            ? 0U
            : 255U;
    }

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
