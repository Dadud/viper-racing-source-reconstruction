#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace viper {

struct TextureResource {
    std::uint8_t flags{};
    std::uint8_t geometry_usage{};
    std::uint8_t wrap{};
    std::uint16_t color_key{};
    std::uint32_t mip_count{};
    std::uint32_t width{};
    std::uint32_t height{};
    // Decoded full-resolution level in row-major RGBA8 order.
    std::vector<std::uint8_t> rgba;
    // Decoded levels in D3D order: full resolution first, down to 1x1.
    // rgba mirrors mip_rgba.front() for existing callers.
    std::vector<std::vector<std::uint8_t>> mip_rgba;
};

// Parses the TEX payload after its 20-byte RES0/TEX /version-3/IMG! envelope.
// Flags 0x00, 0x01 and 0x02 have a supported 16-bit base-level layout. The
// observed 0x03 four-byte layout is deliberately rejected until its channels
// and packing are established.
[[nodiscard]] std::optional<TextureResource> parse_texture_payload(
    std::span<const std::byte> payload);

// Parses a standalone version-3 TEX file with the 20-byte RES0/TEX /IMG!
// envelope used by Config/paintN.tex, then decodes its embedded payload.
[[nodiscard]] std::optional<TextureResource> parse_texture_file(
    std::span<const std::byte> file);

} // namespace viper
