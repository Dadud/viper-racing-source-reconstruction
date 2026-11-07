#include "viper/track_graphics_resource.hpp"

#include <bit>
#include <cmath>
#include <functional>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace viper {
namespace {

constexpr std::size_t kCenterBytes = 16U;
constexpr std::size_t kCornerBytes = 32U;
constexpr std::size_t kMaterialBytes = 32U;
constexpr std::size_t kFaceBytes = 8U;
constexpr std::size_t kMaterialNameBytes = 24U;
constexpr std::size_t kMaterialRangesOffset = 24U;
constexpr std::size_t kMaximumCornersPerChunk = 32768U;
constexpr std::uint32_t kFacingNodeType = 4U;
constexpr std::uint32_t kFacingMarker = 4U;
constexpr std::uint32_t kFacingIdCount = 512U;
constexpr std::uint32_t kKnownNodeTypes[] = {0U, 1U, 2U, 3U, 4U, 7U};

std::uint32_t read_u32(const std::span<const std::byte> bytes,
                       const std::size_t offset) noexcept
{
    return static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset])) |
        (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1U])) << 8U) |
        (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2U])) << 16U) |
        (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3U])) << 24U);
}

std::int16_t read_i16(const std::span<const std::byte> bytes,
                      const std::size_t offset) noexcept
{
    const auto raw = static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(bytes[offset])) |
        static_cast<std::uint16_t>(
            std::to_integer<std::uint8_t>(bytes[offset + 1U]) << 8U);
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(raw));
}

float read_f32(const std::span<const std::byte> bytes,
               const std::size_t offset) noexcept
{
    return std::bit_cast<float>(read_u32(bytes, offset));
}

bool bounded_finite(const float value) noexcept
{
    return std::isfinite(value) && std::abs(value) <= 1.0e6F;
}

std::array<float, 3> cross3(const std::array<float, 3>& left,
                            const std::array<float, 3>& right) noexcept
{
    return {left[1] * right[2] - left[2] * right[1],
        left[2] * right[0] - left[0] * right[2],
        left[0] * right[1] - left[1] * right[0]};
}

bool looks_like_corner(const std::span<const std::byte> bytes,
                       const std::size_t offset) noexcept
{
    if (offset > bytes.size() || bytes.size() - offset < kCornerBytes ||
        read_u32(bytes, offset + 12U) != 0U ||
        (read_u32(bytes, offset + 16U) & 0xff000000U) != 0xff000000U ||
        read_u32(bytes, offset + 20U) != 0xff000000U) {
        return false;
    }
    return bounded_finite(read_f32(bytes, offset)) &&
        bounded_finite(read_f32(bytes, offset + 4U)) &&
        bounded_finite(read_f32(bytes, offset + 8U));
}

struct ParsedChunk {
    TrackGraphicsChunk value;
    std::size_t end_offset{};
};

std::optional<ParsedChunk> parse_chunk_at(
    const std::span<const std::byte> bytes, const std::size_t offset)
{
    if (offset > bytes.size() || bytes.size() - offset < kCenterBytes ||
        !looks_like_corner(bytes, offset + kCenterBytes)) {
        return std::nullopt;
    }

    TrackGraphicsChunk result{};
    result.source_offset = static_cast<std::uint32_t>(offset);
    result.center = {read_f32(bytes, offset), read_f32(bytes, offset + 4U),
        read_f32(bytes, offset + 8U)};
    result.opaque_center_word = read_u32(bytes, offset + 12U);
    if (!bounded_finite(result.center[0]) ||
        !bounded_finite(result.center[1]) ||
        !bounded_finite(result.center[2])) {
        return std::nullopt;
    }
    if (offset >= 0x3cU && read_u32(bytes, offset - 0x3cU) == kFacingNodeType &&
        read_u32(bytes, offset - 4U) == kFacingMarker) {
        if (result.opaque_center_word >= kFacingIdCount) return std::nullopt;
        result.facing_model = true;
        result.facing_id = result.opaque_center_word;
    }

    std::size_t cursor = offset + kCenterBytes;
    while (looks_like_corner(bytes, cursor)) {
        if (result.corners.size() == kMaximumCornersPerChunk) {
            return std::nullopt;
        }
        TrackGraphicsCorner corner{};
        auto delta = std::array<float, 3>{
            read_f32(bytes, cursor), read_f32(bytes, cursor + 4U),
            read_f32(bytes, cursor + 8U)};
        if (result.facing_model) delta = {delta[0], -delta[2], delta[1]};
        for (std::size_t axis = 0; axis < delta.size(); ++axis) {
            corner.position[axis] = result.center[axis] + delta[axis];
            if (!bounded_finite(corner.position[axis])) return std::nullopt;
        }
        corner.packed_color = read_u32(bytes, cursor + 16U);
        corner.uv = {read_f32(bytes, cursor + 24U),
            read_f32(bytes, cursor + 28U)};
        if (!bounded_finite(corner.uv[0]) || !bounded_finite(corner.uv[1])) {
            return std::nullopt;
        }
        result.corners.push_back(corner);
        cursor += kCornerBytes;
    }
    if (result.corners.empty()) return std::nullopt;

    std::uint32_t next_corner = 0U;
    std::uint32_t next_face = 0U;
    std::size_t material_cursor = cursor;
    bool material_chain_complete = false;
    while (material_cursor <= bytes.size() &&
           bytes.size() - material_cursor >= kMaterialBytes) {
        const auto name_field = bytes.subspan(material_cursor, kMaterialNameBytes);
        std::size_t name_length = 0U;
        while (name_length < name_field.size() && name_field[name_length] != std::byte{0}) {
            const auto character = std::to_integer<unsigned char>(name_field[name_length]);
            if (character < 0x20U || character > 0x7eU) return std::nullopt;
            ++name_length;
        }
        if (name_length == name_field.size()) return std::nullopt;

        const auto first_corner = read_i16(bytes, material_cursor + kMaterialRangesOffset);
        const auto end_corner = read_i16(bytes, material_cursor + kMaterialRangesOffset + 2U);
        const auto first_face = read_i16(bytes, material_cursor + kMaterialRangesOffset + 4U);
        const auto end_face = read_i16(bytes, material_cursor + kMaterialRangesOffset + 6U);
        if (first_corner < 0 || end_corner < first_corner || first_face < 0 ||
            end_face < first_face || static_cast<std::uint32_t>(first_corner) != next_corner ||
            static_cast<std::uint32_t>(first_face) != next_face ||
            static_cast<std::size_t>(end_corner) > result.corners.size()) {
            return std::nullopt;
        }

        TrackGraphicsMaterialRange material{};
        material.texture_name.reserve(name_length);
        for (std::size_t index = 0; index < name_length; ++index) {
            material.texture_name.push_back(
                static_cast<char>(std::to_integer<unsigned char>(name_field[index])));
        }
        material.first_corner = static_cast<std::uint32_t>(first_corner);
        material.end_corner = static_cast<std::uint32_t>(end_corner);
        material.first_face = static_cast<std::uint32_t>(first_face);
        material.end_face = static_cast<std::uint32_t>(end_face);
        result.materials.push_back(std::move(material));
        next_corner = static_cast<std::uint32_t>(end_corner);
        next_face = static_cast<std::uint32_t>(end_face);
        material_cursor += kMaterialBytes;
        if (next_corner == static_cast<std::uint32_t>(result.corners.size())) {
            material_chain_complete = true;
            break;
        }
    }
    if (!material_chain_complete || result.materials.empty()) return std::nullopt;

    const auto face_count = static_cast<std::size_t>(next_face);
    if (face_count > (bytes.size() - material_cursor) / kFaceBytes) {
        return std::nullopt;
    }
    result.faces.reserve(face_count);
    for (std::size_t face_index = 0; face_index < face_count; ++face_index) {
        const auto face_offset = material_cursor + face_index * kFaceBytes;
        const auto a = read_i16(bytes, face_offset);
        const auto b = read_i16(bytes, face_offset + 2U);
        const auto c = read_i16(bytes, face_offset + 4U);
        const auto pad = read_i16(bytes, face_offset + 6U);
        if (a < 0 || b < 0 || c < 0 || pad != 0 ||
            static_cast<std::size_t>(a) >= result.corners.size() ||
            static_cast<std::size_t>(b) >= result.corners.size() ||
            static_cast<std::size_t>(c) >= result.corners.size()) {
            return std::nullopt;
        }
        result.faces.push_back({static_cast<std::uint16_t>(a),
            static_cast<std::uint16_t>(b), static_cast<std::uint16_t>(c)});
    }
    const auto end_offset = material_cursor + face_count * kFaceBytes;

    return ParsedChunk{std::move(result), end_offset};
}

void append_opaque_range(TrackGraphicsResourcePayload& output,
                         const std::size_t begin, const std::size_t end)
{
    if (begin == end) return;
    if (begin > std::numeric_limits<std::uint32_t>::max() ||
        end - begin > std::numeric_limits<std::uint32_t>::max()) return;
    output.opaque_ranges.push_back({static_cast<std::uint32_t>(begin),
        static_cast<std::uint32_t>(end - begin)});
}

bool parse_scene_graph(TrackGraphicsResourcePayload& output,
                       const std::span<const std::byte> bytes)
{
    // Walk linked records without assigning one global meaning to +04/+08:
    // GrafLoad's fixup and the renderer's draw_tree use different traversal
    // orders. The renderer-specific order is established from race.bin later.
    std::vector<std::uint32_t> pending{0U};
    std::unordered_set<std::uint32_t> visited;
    bool complete = true;
    while (!pending.empty()) {
        const auto offset = pending.back();
        pending.pop_back();
        if (offset > bytes.size() || bytes.size() - offset < 16U) {
            complete = false;
            continue;
        }
        if (!visited.insert(offset).second) {
            complete = false;
            continue;
        }

        TrackGraphicsSceneNode node{};
        node.source_offset = offset;
        node.type = read_u32(bytes, offset);
        node.link_plus_4_offset = read_u32(bytes, offset + 4U);
        node.link_plus_8_offset = read_u32(bytes, offset + 8U);
        node.payload_word_plus_0c = std::bit_cast<std::int32_t>(
            read_u32(bytes, offset + 12U));

        bool known_type = false;
        for (const auto known : kKnownNodeTypes) known_type |= node.type == known;
        if (!known_type) complete = false;
        if (node.type == 2U) {
            if (bytes.size() - offset < 0x20U) {
                complete = false;
            } else {
                const std::array<float, 3> center{read_f32(bytes, offset + 0x0cU),
                    read_f32(bytes, offset + 0x10U), read_f32(bytes, offset + 0x14U)};
                const auto first = read_f32(bytes, offset + 0x18U);
                const auto second = read_f32(bytes, offset + 0x1cU);
                if (bounded_finite(center[0]) && bounded_finite(center[1]) &&
                    bounded_finite(center[2])) node.lod_center = center;
                if (std::isfinite(first)) node.lod_distance = first;
                if (std::isfinite(second)) node.lod_second_distance = second;
            }
        } else if (node.type == 1U) {
            if (bytes.size() - offset < 0x20U) {
                complete = false;
            } else {
                const std::array<float, 3> center{read_f32(bytes, offset + 0x10U),
                    read_f32(bytes, offset + 0x14U), read_f32(bytes, offset + 0x18U)};
                const auto radius = read_f32(bytes, offset + 0x1cU);
                if (bounded_finite(center[0]) && bounded_finite(center[1]) &&
                    bounded_finite(center[2])) node.visibility_center = center;
                if (bounded_finite(radius) && radius >= 0.0F)
                    node.visibility_radius = radius;
            }
        } else if (node.type == 3U || node.type == 4U) {
            const auto model_header_size = node.type == 3U ? 0x38U : 0x4cU;
            if (bytes.size() - offset < model_header_size) {
                complete = false;
            } else {
                node.vertex_count = read_u32(bytes, offset + 0x10U);
                node.material_count = read_u32(bytes, offset + 0x18U);
                node.face_count = read_u32(bytes, offset + 0x20U);
                // GrafLoad 0x00467CC0 creates the runtime model handle when
                // the material count at node+0x18 is positive; raw +0x0c is
                // not that runtime handle in the serialized payload.
                node.has_runtime_model_handle = node.material_count != 0U;
                const auto extra16_count = read_u32(bytes, offset + 0x28U);
                const auto extra32_count = read_u32(bytes, offset + 0x30U);
                if (node.type == 4U) {
                    node.facing_mode = read_u32(bytes, offset + 0x38U);
                    node.facing_registered = node.facing_mode == 4U;
                    const auto id = read_u32(bytes, offset + 0x48U);
                    if (id < kFacingIdCount) node.facing_id = id;
                }
                if (node.vertex_count != 0U && node.material_count != 0U &&
                    node.face_count != 0U) {
                    const auto center_offset = static_cast<std::uint64_t>(offset) +
                        (node.type == 3U ? 0x28U : 0x3cU);
                    if (center_offset <= std::numeric_limits<std::uint32_t>::max()) {
                        node.geometry_chunk_offset = static_cast<std::uint32_t>(center_offset);
                    } else {
                        complete = false;
                    }
                }
                const auto array_bytes = static_cast<std::uint64_t>(node.vertex_count) * kCornerBytes +
                    static_cast<std::uint64_t>(node.material_count) * kMaterialBytes +
                    static_cast<std::uint64_t>(node.face_count) * kFaceBytes +
                    static_cast<std::uint64_t>(extra16_count) * 16U +
                    static_cast<std::uint64_t>(extra32_count) * 32U;
                const auto array_start = static_cast<std::uint64_t>(offset) + model_header_size;
                if (array_start > bytes.size() || array_bytes > bytes.size() - array_start) {
                    complete = false;
                }
            }
        }

        output.scene_nodes.push_back(node);
        // Preserve the loader-fixup order: recurse through +04, then continue
        // the +08 chain. This order is for inventory only, not draw selection.
        if (node.link_plus_8_offset != 0U) pending.push_back(node.link_plus_8_offset);
        if (node.link_plus_4_offset != 0U) pending.push_back(node.link_plus_4_offset);
    }
    output.scene_graph_complete = complete;
    return complete;
}

} // namespace

std::optional<TrackGraphicsResourcePayload> parse_track_graphics_resource(
    const std::uint32_t resource_version,
    const std::span<const std::byte> payload)
{
    // Native retail tracks use GRAF version 3. Unknown versions stay opaque.
    if (resource_version != 3U || payload.empty() ||
        payload.size() > std::numeric_limits<std::uint32_t>::max()) {
        return std::nullopt;
    }

    TrackGraphicsResourcePayload result{};
    result.resource_version = resource_version;
    result.source_payload_size = static_cast<std::uint32_t>(payload.size());
    result.original_payload.assign(payload.begin(), payload.end());
    (void)parse_scene_graph(result, payload);
    std::size_t cursor = 0U;
    std::size_t opaque_start = 0U;
    while (cursor <= payload.size() && payload.size() - cursor >= kCenterBytes) {
        const auto chunk = parse_chunk_at(payload, cursor);
        if (!chunk) {
            ++cursor;
            continue;
        }
        append_opaque_range(result, opaque_start, cursor);
        result.chunks.push_back(chunk->value);
        cursor = chunk->end_offset;
        opaque_start = cursor;
    }
    append_opaque_range(result, opaque_start, payload.size());
    if (result.chunks.empty()) return std::nullopt;

    std::unordered_map<std::uint32_t, std::size_t> chunks_by_offset;
    chunks_by_offset.reserve(result.chunks.size());
    for (std::size_t index = 0; index < result.chunks.size(); ++index) {
        chunks_by_offset.emplace(result.chunks[index].source_offset, index);
    }
    result.scene_graph_geometry_matched = result.scene_graph_complete;
    std::vector<bool> chunk_referenced(result.chunks.size(), false);
    for (auto& node : result.scene_nodes) {
        if (!node.geometry_chunk_offset) continue;
        const auto found = chunks_by_offset.find(*node.geometry_chunk_offset);
        if (found == chunks_by_offset.end()) {
            result.scene_graph_geometry_matched = false;
            continue;
        }
        const auto& chunk = result.chunks[found->second];
        if (chunk.corners.size() != node.vertex_count ||
            chunk.materials.size() != node.material_count ||
            chunk.faces.size() != node.face_count) {
            result.scene_graph_geometry_matched = false;
            continue;
        }
        if (chunk_referenced[found->second]) {
            result.scene_graph_geometry_matched = false;
            continue;
        }
        chunk_referenced[found->second] = true;
        node.parsed_chunk_index = found->second;
    }
    for (const auto referenced : chunk_referenced) {
        if (!referenced) result.scene_graph_geometry_matched = false;
    }
    for (const auto& node : result.scene_nodes) {
        if (node.type != 4U || node.facing_mode != 4U ||
            !node.has_runtime_model_handle) continue;
        if (!node.facing_id) {
            ++result.facing_registry_invalid_id_count;
            continue;
        }
        auto& slot = result.facing_registry_node_offsets[*node.facing_id];
        if (slot) {
            ++result.facing_registry_duplicate_id_count;
            continue;
        }
        slot = node.source_offset;
    }
    return result;
}

TrackGraphicsInventory summarize_track_graphics(
    const TrackGraphicsResourcePayload& resource) noexcept
{
    TrackGraphicsInventory result{};
    result.chunk_count = resource.chunks.size();
    for (const auto& chunk : resource.chunks) {
        result.corner_count += chunk.corners.size();
        result.face_count += chunk.faces.size();
        result.material_count += chunk.materials.size();
        result.facing_model_count += chunk.facing_model ? 1U : 0U;
        for (const auto& material : chunk.materials) {
            result.empty_material_name_count += material.texture_name.empty() ? 1U : 0U;
        }
    }
    for (const auto& range : resource.opaque_ranges) result.opaque_bytes += range.size;
    result.scene_node_count = resource.scene_nodes.size();
    for (const auto& node : resource.scene_nodes) {
        result.scene_geometry_node_count += node.type == 3U || node.type == 4U ? 1U : 0U;
        result.scene_lod_node_count += node.type == 2U ? 1U : 0U;
        result.scene_facing_node_count += node.type == 4U && node.facing_registered ? 1U : 0U;
        result.scene_geometry_chunks_matched += node.parsed_chunk_index.has_value() ? 1U : 0U;
    }
    result.facing_registry_duplicate_id_count =
        resource.facing_registry_duplicate_id_count;
    result.facing_registry_invalid_id_count =
        resource.facing_registry_invalid_id_count;
    for (const auto& slot : resource.facing_registry_node_offsets)
        result.facing_registry_entry_count += slot.has_value() ? 1U : 0U;
    return result;
}

std::optional<std::uint32_t> lookup_registered_facing_node(
    const TrackGraphicsResourcePayload& resource,
    const std::int32_t facing_id) noexcept
{
    if (facing_id < 0 || facing_id >=
        static_cast<std::int32_t>(resource.facing_registry_node_offsets.size())) {
        return std::nullopt;
    }
    return resource.facing_registry_node_offsets[
        static_cast<std::size_t>(facing_id)];
}

TrackGraphicsNodeSelection select_track_graphics_nodes(
    const TrackGraphicsResourcePayload& resource,
    const TrackGraphicsCamera& camera)
{
    TrackGraphicsNodeSelection result{};
    result.traversal_complete = resource.scene_graph_complete &&
        resource.scene_graph_geometry_matched &&
        std::isfinite(camera.axial_far_distance) &&
        std::isfinite(camera.lod_bias_squared) &&
        std::isfinite(camera.lod_scale_squared) &&
        std::isfinite(camera.facing_max_distance_squared) &&
        std::isfinite(camera.facing_mode_2_min_distance_squared) &&
        camera.lod_scale_squared >= 0.0F;
    for (const auto value : camera.position) result.traversal_complete &= std::isfinite(value);
    for (const auto value : camera.forward) result.traversal_complete &= std::isfinite(value);
    if (!result.traversal_complete) return result;

    std::unordered_map<std::uint32_t, const TrackGraphicsSceneNode*> nodes_by_offset;
    nodes_by_offset.reserve(resource.scene_nodes.size());
    for (const auto& node : resource.scene_nodes) {
        if (!nodes_by_offset.emplace(node.source_offset, &node).second) {
            result.traversal_complete = false;
            return result;
        }
    }

    std::unordered_set<std::uint32_t> visited;
    visited.reserve(resource.scene_nodes.size());
    std::function<void(std::uint32_t, bool)> walk_chain;
    walk_chain = [&](const std::uint32_t first_offset, const bool include_root) {
        auto current_offset = first_offset;
        bool first_node = true;
        while ((current_offset != 0U || (first_node && include_root)) &&
               result.traversal_complete) {
            const auto found = nodes_by_offset.find(current_offset);
            if (found == nodes_by_offset.end() || !visited.insert(current_offset).second) {
                result.traversal_complete = false;
                return;
            }
            ++result.visited_node_count;
            const auto& node = *found->second;
            if (node.type < result.visited_type_counts.size())
                ++result.visited_type_counts[node.type];
            first_node = false;
            bool visit_branch = true;
            switch (node.type) {
            case 1U:
                if (node.payload_word_plus_0c != 0) {
                    if (!node.visibility_center || !node.visibility_radius) {
                        result.traversal_complete = false;
                        return;
                    }
                    const auto& center = *node.visibility_center;
                    const float depth =
                        (center[0] - camera.position[0]) * camera.forward[0] +
                        (center[1] - camera.position[1]) * camera.forward[1] +
                        (center[2] - camera.position[2]) * camera.forward[2];
                    const float radius = *node.visibility_radius;
                    visit_branch = std::isfinite(depth) &&
                        depth >= -radius &&
                        depth < camera.axial_far_distance + radius;
                }
                break;
            case 2U:
                if (!node.lod_distance || !node.lod_second_distance) {
                    result.traversal_complete = false;
                    return;
                }
                // Retail 0x00467B10 skips the gated branch when +0x1c is -1.0.
                if (*node.lod_second_distance == -1.0F) {
                    visit_branch = false;
                    break;
                }
                if (!node.lod_center) {
                    result.traversal_complete = false;
                    return;
                }
                {
                    const auto& center = *node.lod_center;
                    const float dx = center[0] - camera.position[0];
                    const float dy = center[1] - camera.position[1];
                    const float dz = center[2] - camera.position[2];
                    const float distance_squared = dx * dx + dy * dy + dz * dz;
                    const float adjusted_distance_squared =
                        (distance_squared + camera.lod_bias_squared) *
                        camera.lod_scale_squared;
                    const float lower_squared =
                        *node.lod_second_distance * *node.lod_second_distance;
                    const float upper_squared =
                        *node.lod_distance * *node.lod_distance;
                    visit_branch = std::isfinite(adjusted_distance_squared) &&
                        adjusted_distance_squared >= lower_squared &&
                        adjusted_distance_squared < upper_squared;
                }
                break;
            case 3U:
                if (node.has_runtime_model_handle && node.parsed_chunk_index)
                    result.ordinary_node_offsets.push_back(node.source_offset);
                visit_branch = false;
                break;
            case 4U:
                if (node.has_runtime_model_handle && node.parsed_chunk_index) {
                    if (node.facing_mode == 4U) {
                        if (lookup_registered_facing_node(resource,
                                node.facing_id
                                    ? static_cast<std::int32_t>(*node.facing_id) : -1) ==
                            node.source_offset) {
                            result.registered_mode_4_node_offsets.push_back(
                                node.source_offset);
                            const auto& center = resource.chunks[
                                *node.parsed_chunk_index].center;
                            result.registered_mode_4_transforms.push_back({
                                node.source_offset,
                                {1.0F, 0.0F, 0.0F,
                                 0.0F, 1.0F, 0.0F,
                                 0.0F, 0.0F, 1.0F,
                                 center[0], center[1], center[2]}});
                        }
                    } else if (node.facing_mode == 1U || node.facing_mode == 2U) {
                        const auto& center = resource.chunks[
                            *node.parsed_chunk_index].center;
                        const float dx = center[0] - camera.position[0];
                        const float dy = center[1] - camera.position[1];
                        const float dz = center[2] - camera.position[2];
                        const float distance_squared =
                            dx * dx + dy * dy + dz * dz;
                        if (!std::isfinite(distance_squared)) {
                            result.traversal_complete = false;
                            return;
                        }
                        // Native 0x00467790 uses the 90000 squared-distance
                        // cutoff for both facing queues. Mode 2 additionally
                        // avoids its singularity inside a 0.1-unit radius.
                        if (distance_squared < camera.facing_max_distance_squared &&
                            (node.facing_mode != 2U || distance_squared >
                                camera.facing_mode_2_min_distance_squared)) {
                            if (node.facing_mode == 1U) {
                                result.facing_mode_1_node_offsets.push_back(
                                    node.source_offset);
                                const float forward_length_squared =
                                    camera.forward[0] * camera.forward[0] +
                                    camera.forward[1] * camera.forward[1] +
                                    camera.forward[2] * camera.forward[2];
                                if (forward_length_squared <= 1.0e-12F) {
                                    result.traversal_complete = false;
                                    return;
                                }
                                const float inverse_forward_length =
                                    1.0F / std::sqrt(forward_length_squared);
                                std::array<float, 3> forward{
                                    camera.forward[0] * inverse_forward_length,
                                    camera.forward[1] * inverse_forward_length,
                                    camera.forward[2] * inverse_forward_length};
                                const std::array<float, 3> world_up{0.0F, 1.0F, 0.0F};
                                auto right = cross3(world_up, forward);
                                const float right_length_squared = right[0] * right[0] +
                                    right[1] * right[1] + right[2] * right[2];
                                if (right_length_squared <= 1.0e-10F) {
                                    right = {1.0F, 0.0F, 0.0F};
                                } else {
                                    const float inverse_right_length =
                                        1.0F / std::sqrt(right_length_squared);
                                    for (auto& component : right)
                                        component *= inverse_right_length;
                                }
                                const auto up = cross3(forward, right);
                                // The native mode-1 path keeps camera-up,
                                // negates camera-back, then computes up x forward.
                                const auto native_right = cross3(up, forward);
                                result.facing_mode_1_transforms.push_back({
                                    node.source_offset,
                                    {native_right[0], native_right[1], native_right[2],
                                     up[0], up[1], up[2],
                                     forward[0], forward[1], forward[2],
                                     center[0], center[1], center[2]}});
                            } else {
                                result.facing_mode_2_node_offsets.push_back(
                                    node.source_offset);
                                const float inverse_distance =
                                    1.0F / std::sqrt(distance_squared);
                                const std::array<float, 3> to_camera{
                                    -dx * inverse_distance, -dy * inverse_distance,
                                    -dz * inverse_distance};
                                const std::array<float, 3> world_up{0.0F, 1.0F, 0.0F};
                                auto right = cross3(world_up, to_camera);
                                const float right_length_squared = right[0] * right[0] +
                                    right[1] * right[1] + right[2] * right[2];
                                if (right_length_squared <=
                                    camera.facing_mode_2_min_distance_squared) {
                                    right = {1.0F, 0.0F, 0.0F};
                                } else {
                                    const float inverse_right_length =
                                        1.0F / std::sqrt(right_length_squared);
                                    for (auto& component : right)
                                        component *= inverse_right_length;
                                }
                                const auto third = cross3(right, world_up);
                                result.facing_mode_2_transforms.push_back({
                                    node.source_offset,
                                    {right[0], right[1], right[2],
                                     world_up[0], world_up[1], world_up[2],
                                     third[0], third[1], third[2],
                                     center[0], center[1], center[2]}});
                            }
                        }
                    } else {
                        result.traversal_complete = false;
                    }
                }
                visit_branch = false;
                break;
            default:
                break;
            }
            if (!result.traversal_complete) return;
            if (visit_branch && node.link_plus_8_offset != 0U)
                walk_chain(node.link_plus_8_offset, false);
            current_offset = node.link_plus_4_offset;
        }
    };

    if (nodes_by_offset.empty() || nodes_by_offset.find(0U) == nodes_by_offset.end()) {
        result.traversal_complete = false;
        return result;
    }
    walk_chain(0U, true);
    return result;
}

std::vector<std::uint32_t> select_track_wobl_node_offsets(
    const TrackGraphicsResourcePayload& resource,
    const TrackGraphicsNodeSelection& selection,
    const std::span<const std::uint32_t> wobl_resource_ids)
{
    std::vector<std::uint32_t> result;
    result.reserve(wobl_resource_ids.size());
    for (const auto resource_id : wobl_resource_ids) {
        if (resource_id >= resource.facing_registry_node_offsets.size()) continue;
        const auto node_offset = lookup_registered_facing_node(
            resource, static_cast<std::int32_t>(resource_id));
        if (!node_offset || std::find(
                selection.registered_mode_4_node_offsets.begin(),
                selection.registered_mode_4_node_offsets.end(), *node_offset) ==
                selection.registered_mode_4_node_offsets.end()) {
            continue;
        }
        result.push_back(*node_offset);
    }
    return result;
}

} // namespace viper
