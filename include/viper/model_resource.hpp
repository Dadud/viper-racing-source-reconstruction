#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace viper {

struct ModelVertex {
    // Eight packed little-endian float fields as stored in 32-byte FNIM rows.
    // Position/normal/UV interpretation is provisional pending GPU declaration evidence.
    std::array<float, 8> fields{};
};

struct ModelMaterialGroup {
    std::string material_name;
    std::uint16_t first_vertex{};
    std::uint16_t end_vertex{};
    std::uint16_t first_triangle{};
    std::uint16_t end_triangle{};
    // Indices are local to [first_vertex, end_vertex), as emitted by the native decoder.
    std::vector<std::uint16_t> local_indices;
};

struct ModelResource {
    static constexpr std::uint32_t resource_type_code = 0x4d494e46U; // FNIM
    static constexpr std::size_t header_size = 40U;
    std::uint32_t resource_version{};
    std::array<std::uint32_t, 5> section_counts{};
    std::vector<ModelVertex> vertices;
    std::vector<ModelMaterialGroup> material_groups;
};

struct ModelVertexBounds {
    std::array<float, 3> minimum{};
    std::array<float, 3> maximum{};
    std::size_t vertex_count{};
};

// Reconstructs the XYZ extrema loop in race.bin 0x4509B0 over 32-byte rows.
[[nodiscard]] ModelVertexBounds measure_model_vertex_bounds(
    std::span<const ModelVertex> vertices) noexcept;

// Parses FNIM section counts/strides and builds each material group's local
// triangle index stream. Only resource versions 0 and 1 are supported.
[[nodiscard]] std::optional<ModelResource> parse_model_resource(
    std::uint32_t resource_version,
    std::span<const std::byte> payload);

} // namespace viper
