#include "viper/terrain_surface.hpp"
#include "viper/resource_bundle.hpp"
#include "viper/legacy_fdiv.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace viper {
namespace {

constexpr float quantization_step = 0.001F;
constexpr float quantization_bias = 0.5F;
constexpr float split_threshold = 0.0F;
constexpr float minimum_vertical_normal = 0x1.0p-23F;
constexpr float segment_side_epsilon = 0.001F;
constexpr float segment_parameter_slack = 0.0001F;

std::uint32_t read_u32_le(const std::span<const std::byte> bytes,
                          const std::size_t offset) noexcept
{
    return static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset])) |
           (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1U])) << 8U) |
           (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2U])) << 16U) |
           (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3U])) << 24U);
}

float read_f32_le(const std::span<const std::byte> bytes,
                  const std::size_t offset) noexcept
{
    return std::bit_cast<float>(read_u32_le(bytes, offset));
}

std::int32_t quantize_coordinate(const float coordinate) noexcept
{
    const auto scaled = static_cast<long double>(coordinate) /
        static_cast<long double>(quantization_step) +
        static_cast<long double>(quantization_bias);
    constexpr auto low = static_cast<long double>(
        std::numeric_limits<std::int64_t>::min());
    constexpr auto high_exclusive = -low;
    if (!std::isfinite(scaled) || scaled < low || scaled >= high_exclusive) {
        // 0x4BD1F8's invalid FISTP qword sentinel has a zero low DWORD, which
        // is the only part stored by the terrain coordinate quantizer.
        return 0;
    }
    const auto ticks = static_cast<std::int64_t>(std::trunc(scaled));
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(ticks));
}

float quantized_coordinate(const float coordinate) noexcept
{
    const auto ticks = quantize_coordinate(coordinate);
    return static_cast<float>(static_cast<long double>(ticks) *
                              static_cast<long double>(quantization_step));
}

float partition_side(const TerrainPartitionNode& node,
                     const std::array<float, 3>& point) noexcept
{
    return node.split_x_coefficient * point[0] +
        node.split_z_coefficient * point[2] + node.split_offset;
}

bool point_in_terrain_triangle(const TerrainSurfacePlane& plane,
                               const float x, const float z) noexcept
{
    const auto& v = plane.trailing_values;
    const float ax = v[0] - x, az = v[2] - z;
    const float bx = v[3] - x, bz = v[5] - z;
    const float cx = v[6] - x, cz = v[8] - z;
    const std::array<float, 3> edges{
        bx * az - bz * ax,
        cx * bz - cz * bx,
        cz * ax - cx * az};
    // 0x466DB0 accepts one consistent orientation with a 0.0001 absolute
    // edge-cross tolerance. Preserve its dword-order comparisons directly.
    const bool orientation_a = std::all_of(edges.begin(), edges.end(), [](const float edge) {
        return std::bit_cast<std::uint32_t>(edge) < 0xb8d1b718U;
    });
    const bool orientation_b = std::all_of(edges.begin(), edges.end(), [](const float edge) {
        return std::bit_cast<std::int32_t>(edge) < 0x38d1b718;
    });
    return orientation_a || orientation_b;
}

} // namespace

std::optional<TerrainSurfaceResourcePayload> parse_terrain_surface_payload(
    const std::uint32_t resource_version,
    const std::span<const std::byte> payload)
{
    using Terrain = TerrainSurfaceResourcePayload;
    if (resource_version != Terrain::supported_resource_version ||
        payload.size() < Terrain::header_size) {
        return std::nullopt;
    }

    TerrainSurfaceResourcePayload result{};
    result.resource_version = resource_version;
    for (std::size_t index = 0; index < result.raw_header_words.size(); ++index) {
        result.raw_header_words[index] = read_u32_le(payload, index * sizeof(std::uint32_t));
    }

    const auto plane_count = static_cast<std::size_t>(result.raw_header_words[0]);
    const auto plane_bytes_available = payload.size() - Terrain::header_size;
    if (plane_count > plane_bytes_available / Terrain::plane_record_size) {
        return std::nullopt;
    }
    const auto node_offset = Terrain::header_size + plane_count * Terrain::plane_record_size;
    const auto node_bytes = payload.size() - node_offset;
    if (node_bytes == 0U || node_bytes % Terrain::partition_node_size != 0U) {
        return std::nullopt;
    }
    const auto node_count = node_bytes / Terrain::partition_node_size;
    const auto root_index = std::bit_cast<std::int32_t>(result.raw_header_words[4]);
    if (root_index < 0 || static_cast<std::size_t>(root_index) >= node_count) {
        return std::nullopt;
    }
    result.root_node_index = root_index;

    result.planes.reserve(plane_count);
    for (std::size_t index = 0; index < plane_count; ++index) {
        const auto base = Terrain::header_size + index * Terrain::plane_record_size;
        TerrainSurfacePlane plane{};
        plane.normal_x = read_f32_le(payload, base);
        plane.normal_y = read_f32_le(payload, base + 4U);
        plane.normal_z = read_f32_le(payload, base + 8U);
        plane.offset = read_f32_le(payload, base + 12U);
        for (std::size_t trailing = 0; trailing < plane.trailing_values.size(); ++trailing) {
            plane.trailing_values[trailing] = read_f32_le(
                payload, base + 16U + trailing * sizeof(float));
        }
        const auto short_pair = read_u32_le(payload, base + 0x34U);
        plane.surface_value_0x34 = std::bit_cast<std::int16_t>(
            static_cast<std::uint16_t>(short_pair & 0xffffU));
        plane.trailing_short_0x36 = static_cast<std::uint16_t>(short_pair >> 16U);
        result.planes.push_back(plane);
    }

    result.partition_nodes.reserve(node_count);
    for (std::size_t index = 0; index < node_count; ++index) {
        const auto base = node_offset + index * Terrain::partition_node_size;
        TerrainPartitionNode node{};
        node.split_x_coefficient = read_f32_le(payload, base);
        node.split_z_coefficient = read_f32_le(payload, base + 4U);
        node.split_offset = read_f32_le(payload, base + 8U);
        node.negative_leaf_index = std::bit_cast<std::int32_t>(read_u32_le(payload, base + 12U));
        node.positive_leaf_index = std::bit_cast<std::int32_t>(read_u32_le(payload, base + 16U));
        node.negative_child_index = std::bit_cast<std::int32_t>(read_u32_le(payload, base + 20U));
        node.positive_child_index = std::bit_cast<std::int32_t>(read_u32_le(payload, base + 24U));
        if (node.negative_child_index < -1 ||
            node.positive_child_index < -1 ||
            (node.negative_child_index >= 0 &&
                static_cast<std::size_t>(node.negative_child_index) >= node_count) ||
            (node.positive_child_index >= 0 &&
                static_cast<std::size_t>(node.positive_child_index) >= node_count)) {
            return std::nullopt;
        }
        result.partition_nodes.push_back(node);
    }
    return result;
}

std::optional<TerrainSurfaceResourcePayload> parse_track_terrain_resource(
    const ResourceBundle& bundle)
{
    const auto* entry = bundle.find("track.bpp",
        TerrainSurfaceResourcePayload::resource_type_code);
    if (entry == nullptr) {
        return std::nullopt;
    }
    return parse_terrain_surface_payload(entry->attributes, bundle.payload(*entry));
}

TerrainSurfaceQueryInput make_terrain_surface_query_input(
    const TerrainSurfaceResourcePayload& terrain,
    const std::array<float, 3> position,
    const bool compatibility_division_enabled) noexcept
{
    return {position, terrain.partition_nodes, terrain.planes,
            terrain.root_node_index, compatibility_division_enabled};
}

TerrainSurfaceQueryResult sample_terrain_resource(
    const TerrainSurfaceResourcePayload& terrain,
    const std::array<float, 3> position,
    const bool compatibility_division_enabled) noexcept
{
    return sample_terrain_surface(make_terrain_surface_query_input(
        terrain, position, compatibility_division_enabled));
}

TerrainSegmentQueryResult query_terrain_resource_segment(
    const TerrainSurfaceResourcePayload& terrain,
    const std::array<float, 3>& start,
    const std::array<float, 3>& end,
    const bool compatibility_division_enabled) noexcept
{
    return query_terrain_segment({start, end, terrain.partition_nodes,
        terrain.planes, terrain.root_node_index, compatibility_division_enabled});
}

TerrainSegmentQueryResult query_terrain_segment(
    const TerrainSegmentQueryInput& input) noexcept
{
    TerrainSegmentQueryResult result{};
    result.compatibility_division_path = input.compatibility_division_enabled;
    if (input.root_node_index < 0 || input.partition_nodes.empty() ||
        static_cast<std::size_t>(input.root_node_index) >= input.partition_nodes.size()) {
        return result;
    }

    constexpr float plane_epsilon = 0.001F;
    const auto try_leaf = [&](const std::int32_t leaf) {
        result.partition_tree_was_valid = true;
        if (leaf < 0 || static_cast<std::size_t>(leaf) >= input.planes.size()) return false;
        const auto& plane = input.planes[static_cast<std::size_t>(leaf)];
        result.selected_plane_index = leaf;
        result.plane_was_valid = true;
        const auto distance = [&plane](const std::array<float, 3>& p) {
            return plane.normal_x * p[0] + plane.normal_y * p[1] +
                plane.normal_z * p[2] + plane.offset;
        };
        const float d0 = distance(input.start), d1 = distance(input.end);
        float t{};
        if (std::abs(d0) < plane_epsilon) t = 0.0F;
        else if (std::abs(d1) < plane_epsilon) t = 1.0F;
        else {
            const float denominator = d1 - d0;
            if (std::abs(denominator) < plane_epsilon) return false;
            t = static_cast<float>(divide_with_vc98_fdiv_compatibility(
                -static_cast<long double>(d0),
                static_cast<long double>(denominator),
                input.compatibility_division_enabled));
        }
        if (t < -segment_parameter_slack ||
            std::bit_cast<std::int32_t>(t) >= 0x3f800348) return false;
        const std::array<float, 3> point{
            input.start[0] + (input.end[0] - input.start[0]) * t,
            input.start[1] + (input.end[1] - input.start[1]) * t,
            input.start[2] + (input.end[2] - input.start[2]) * t};
        if (!point_in_terrain_triangle(plane, point[0], point[2])) return false;
        result.query_succeeded = true;
        result.triangle_was_valid = true;
        result.intersection = point;
        result.plane_normal = {plane.normal_x, plane.normal_y, plane.normal_z};
        result.segment_parameter = t;
        result.surface_value_0x34 = plane.surface_value_0x34;
        return true;
    };
    const auto traverse = [&](auto&& self, const std::int32_t index,
                              const std::size_t depth) -> bool {
        if (depth >= input.partition_nodes.size() || index < 0 ||
            static_cast<std::size_t>(index) >= input.partition_nodes.size()) return false;
        const auto& node = input.partition_nodes[static_cast<std::size_t>(index)];
        float start_side = partition_side(node, input.start);
        float end_side = partition_side(node, input.end);
        if (std::abs(start_side) < segment_side_epsilon) start_side = 0.0F;
        if (std::abs(end_side) < segment_side_epsilon) end_side = 0.0F;
        const bool positive_first = start_side > 0.0F;
        const auto primary_child = positive_first
            ? node.positive_child_index : node.negative_child_index;
        const auto primary_leaf = positive_first
            ? node.positive_leaf_index : node.negative_leaf_index;
        const bool primary_hit = primary_child == -1
            ? try_leaf(primary_leaf)
            : self(self, primary_child, depth + 1U);
        if (primary_hit) return true;
        const bool crosses_to_other_side = positive_first
            ? end_side <= 0.0F : end_side >= 0.0F;
        if (!crosses_to_other_side) return false;
        const auto secondary_child = positive_first
            ? node.negative_child_index : node.positive_child_index;
        const auto secondary_leaf = positive_first
            ? node.negative_leaf_index : node.positive_leaf_index;
        return secondary_child == -1
            ? try_leaf(secondary_leaf)
            : self(self, secondary_child, depth + 1U);
    };
    result.query_succeeded = traverse(traverse, input.root_node_index, 0U);
    return result;
}

float terrain_height_or_zero(
    const TerrainSurfaceResourcePayload& terrain,
    const float x,
    const float z,
    const bool compatibility_division_enabled) noexcept
{
    const auto result = sample_terrain_resource(
        terrain, {x, 1000.0F, z}, compatibility_division_enabled);
    return result.query_succeeded ? result.sampled_position[1] : 0.0F;
}

TerrainSurfaceQueryResult sample_terrain_surface(
    const TerrainSurfaceQueryInput& input) noexcept
{
    TerrainSurfaceQueryResult result{};
    result.compatibility_division_path =
        input.compatibility_division_enabled;
    for (std::size_t axis = 0; axis < result.quantized_lookup_position.size();
         ++axis) {
        result.quantized_lookup_position[axis] =
            quantized_coordinate(input.position[axis]);
    }

    if (input.root_node_index < 0 ||
        static_cast<std::size_t>(input.root_node_index) >=
            input.partition_nodes.size() || input.partition_nodes.empty()) {
        return result;
    }

    auto node_index = input.root_node_index;
    for (std::size_t visited = 0;
         visited < input.partition_nodes.size(); ++visited) {
        if (node_index < 0 || static_cast<std::size_t>(node_index) >=
                input.partition_nodes.size()) {
            return result;
        }
        const auto& node = input.partition_nodes[
            static_cast<std::size_t>(node_index)];
        const auto side_value =
            static_cast<long double>(node.split_x_coefficient) *
                static_cast<long double>(
                    result.quantized_lookup_position[0]) +
            static_cast<long double>(node.split_z_coefficient) *
                static_cast<long double>(
                    result.quantized_lookup_position[2]) +
            static_cast<long double>(node.split_offset);
        if (side_value > static_cast<long double>(split_threshold)) {
            if (node.positive_child_index != -1) {
                node_index = node.positive_child_index;
                continue;
            }
            result.selected_plane_index = node.positive_leaf_index;
        } else {
            if (node.negative_child_index != -1) {
                node_index = node.negative_child_index;
                continue;
            }
            result.selected_plane_index = node.negative_leaf_index;
        }
        result.partition_tree_was_valid = true;
        break;
    }
    if (!result.partition_tree_was_valid || result.selected_plane_index < 0 ||
        static_cast<std::size_t>(result.selected_plane_index) >=
            input.planes.size()) {
        return result;
    }

    const auto& plane = input.planes[
        static_cast<std::size_t>(result.selected_plane_index)];
    if (!(std::abs(plane.normal_y) > minimum_vertical_normal)) {
        return result;
    }

    const auto height_numerator =
        static_cast<long double>(input.position[0]) *
            static_cast<long double>(plane.normal_x) +
        static_cast<long double>(input.position[2]) *
            static_cast<long double>(plane.normal_z) +
        static_cast<long double>(plane.offset);
    const auto height = -height_numerator /
        static_cast<long double>(plane.normal_y);
    result.sampled_position = {
        input.position[0],
        static_cast<float>(height),
        input.position[2],
    };
    result.plane_normal = {plane.normal_x, plane.normal_y, plane.normal_z};
    result.surface_value_0x34 = plane.surface_value_0x34;
    result.plane_was_valid = true;
    result.query_succeeded = true;
    return result;
}

} // namespace viper
