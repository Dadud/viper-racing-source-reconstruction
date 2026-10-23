#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace viper {

class ResourceBundle;

struct TerrainPartitionNode {
    float split_x_coefficient{};
    float split_z_coefficient{};
    float split_offset{};
    std::int32_t negative_leaf_index{-1};
    std::int32_t positive_leaf_index{-1};
    std::int32_t negative_child_index{-1};
    std::int32_t positive_child_index{-1};
};

struct TerrainSurfacePlane {
    float normal_x{};
    float normal_y{};
    float normal_z{};
    float offset{};
    // The original 13-float record has nine additional values that this
    // height query does not read; retain their raw values for reconstruction.
    std::array<float, 9> trailing_values{};
    std::int16_t surface_value_0x34{};
    std::uint16_t trailing_short_0x36{};
};

struct TerrainSurfaceQueryInput {
    std::array<float, 3> position{};
    std::span<const TerrainPartitionNode> partition_nodes;
    std::span<const TerrainSurfacePlane> planes;
    std::int32_t root_node_index{};
    bool compatibility_division_enabled{};
};

struct TerrainSurfaceResourcePayload {
    // The loader compares the native DWORD 0x42505054 (memory bytes "TPPB";
    // the resource-bundle type is conventionally written "BPPT").
    static constexpr std::uint32_t resource_type_code = 0x42505054U;
    static constexpr std::uint32_t supported_resource_version = 2U;
    static constexpr std::size_t header_size = 0x14U;
    static constexpr std::size_t plane_record_size = 0x38U;
    static constexpr std::size_t partition_node_size = 0x1cU;

    std::uint32_t resource_version{};
    std::array<std::uint32_t, 5> raw_header_words{};
    std::int32_t root_node_index{-1};
    std::vector<TerrainSurfacePlane> planes;
    std::vector<TerrainPartitionNode> partition_nodes;
};

// Parses the TPPB payload loaded by 0x467060 and converts the serialized
// child indices used by 0x467130 into the portable node-index representation.
[[nodiscard]] std::optional<TerrainSurfaceResourcePayload>
parse_terrain_surface_payload(
    std::uint32_t resource_version,
    std::span<const std::byte> payload);

// Loads the track.bpp TPPB resource through the bundle index and parser.
[[nodiscard]] std::optional<TerrainSurfaceResourcePayload>
parse_track_terrain_resource(const ResourceBundle& bundle);

// Creates non-owning spans into a parsed payload for the shared query helper.
[[nodiscard]] TerrainSurfaceQueryInput make_terrain_surface_query_input(
    const TerrainSurfaceResourcePayload& terrain,
    std::array<float, 3> position,
    bool compatibility_division_enabled) noexcept;

struct TerrainSurfaceQueryResult {
    bool query_succeeded{};
    bool partition_tree_was_valid{};
    bool plane_was_valid{};
    bool compatibility_division_path{};
    std::int32_t selected_plane_index{-1};
    std::array<float, 3> quantized_lookup_position{};
    std::array<float, 3> sampled_position{};
    std::array<float, 3> plane_normal{};
    std::int16_t surface_value_0x34{};
};

struct TerrainSegmentQueryInput {
    std::array<float, 3> start{};
    std::array<float, 3> end{};
    std::span<const TerrainPartitionNode> partition_nodes;
    std::span<const TerrainSurfacePlane> planes;
    std::int32_t root_node_index{};
    bool compatibility_division_enabled{};
};

struct TerrainSegmentQueryResult {
    bool query_succeeded{};
    bool partition_tree_was_valid{};
    bool plane_was_valid{};
    bool triangle_was_valid{};
    bool compatibility_division_path{};
    std::int32_t selected_plane_index{-1};
    std::array<float, 3> intersection{};
    std::array<float, 3> plane_normal{};
    float segment_parameter{};
    std::int16_t surface_value_0x34{};
};

// Reconstructs 0x4672B0 and its 0x467260 partition walk. The partition
// records and 13-float plane rows come from the caller's loaded track data.
[[nodiscard]] TerrainSurfaceQueryResult sample_terrain_surface(
    const TerrainSurfaceQueryInput& input) noexcept;

// Samples directly from a parsed track resource without manual span wiring.
[[nodiscard]] TerrainSurfaceQueryResult sample_terrain_resource(
    const TerrainSurfaceResourcePayload& terrain,
    std::array<float, 3> position,
    bool compatibility_division_enabled) noexcept;

// Reconstructs the segment/plane/triangle lookup used by 0x45F700.
[[nodiscard]] TerrainSegmentQueryResult query_terrain_segment(
    const TerrainSegmentQueryInput& input) noexcept;

[[nodiscard]] TerrainSegmentQueryResult query_terrain_resource_segment(
    const TerrainSurfaceResourcePayload& terrain,
    const std::array<float, 3>& start,
    const std::array<float, 3>& end,
    bool compatibility_division_enabled) noexcept;

// Matches the 0x45F600 helper: query (x, 1000, z), return sampled Y, or 0 on
// lookup failure. Used by the camera and several track-object constructors.
[[nodiscard]] float terrain_height_or_zero(
    const TerrainSurfaceResourcePayload& terrain,
    float x,
    float z,
    bool compatibility_division_enabled) noexcept;

} // namespace viper
