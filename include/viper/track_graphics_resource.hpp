#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace viper {

struct TrackGraphicsCorner {
    std::array<float, 3> position{};
    std::uint32_t packed_color{};
    std::array<float, 2> uv{};
};

struct TrackGraphicsMaterialRange {
    std::string texture_name;
    std::uint32_t first_corner{};
    std::uint32_t end_corner{};
    std::uint32_t first_face{};
    std::uint32_t end_face{};
};

struct TrackGraphicsChunk {
    std::array<float, 3> center{};
    std::uint32_t opaque_center_word{};
    std::uint32_t source_offset{};
    std::vector<TrackGraphicsCorner> corners;
    std::vector<TrackGraphicsMaterialRange> materials;
    std::vector<std::array<std::uint16_t, 3>> faces;
    bool facing_model{};
    std::optional<std::uint32_t> facing_id;
};

struct TrackGraphicsOpaqueRange {
    std::uint32_t offset{};
    std::uint32_t size{};
};

struct TrackGraphicsSceneNode {
    std::uint32_t source_offset{};
    std::uint32_t type{};
    // Preserve serialized links neutrally: retail draw_tree and the loader's
    // pointer-fixup pass walk these two fields in different orders.
    std::uint32_t link_plus_4_offset{};
    std::uint32_t link_plus_8_offset{};
    std::int32_t payload_word_plus_0c{};
    std::optional<std::array<float, 3>> lod_center{};
    std::optional<float> lod_distance{};
    std::optional<float> lod_second_distance{};
    std::optional<std::array<float, 3>> visibility_center{};
    std::optional<float> visibility_radius{};
    std::uint32_t facing_mode{};
    bool has_runtime_model_handle{};
    std::uint32_t vertex_count{};
    std::uint32_t material_count{};
    std::uint32_t face_count{};
    std::optional<std::uint32_t> geometry_chunk_offset{};
    std::optional<std::size_t> parsed_chunk_index{};
    bool facing_registered{};
    std::optional<std::uint32_t> facing_id{};
};

struct TrackGraphicsResourcePayload {
    static constexpr std::uint32_t resource_type_code = 0x47524146U; // GRAF
    std::uint32_t resource_version{};
    std::uint32_t source_payload_size{};
    std::vector<std::byte> original_payload;
    std::vector<TrackGraphicsChunk> chunks;
    // Linked native scene-graph nodes in a bounded loader-fixup traversal.
    // Raw links are retained so render traversal can apply its own semantics.
    std::vector<TrackGraphicsSceneNode> scene_nodes;
    // Native 0x00467770 lookup table: mode-4 IDs address the first loaded
    // model handle registered for that ID; invalid IDs and duplicates do not
    // replace an existing slot.
    std::array<std::optional<std::uint32_t>, 512> facing_registry_node_offsets{};
    std::size_t facing_registry_duplicate_id_count{};
    std::size_t facing_registry_invalid_id_count{};
    bool scene_graph_complete{};
    bool scene_graph_geometry_matched{};
    // GRF files contain scene-graph records between geometry chunks. Their
    // meaning is not inferred here; ranges preserve where that data remains.
    std::vector<TrackGraphicsOpaqueRange> opaque_ranges;
};

struct TrackGraphicsInventory {
    std::size_t chunk_count{};
    std::size_t corner_count{};
    std::size_t face_count{};
    std::size_t material_count{};
    std::size_t empty_material_name_count{};
    std::size_t facing_model_count{};
    std::size_t opaque_bytes{};
    std::size_t scene_node_count{};
    std::size_t scene_geometry_node_count{};
    std::size_t scene_lod_node_count{};
    std::size_t scene_facing_node_count{};
    std::size_t scene_geometry_chunks_matched{};
    std::size_t facing_registry_entry_count{};
    std::size_t facing_registry_duplicate_id_count{};
    std::size_t facing_registry_invalid_id_count{};
};

struct TrackGraphicsCamera {
    std::array<float, 3> position{};
    std::array<float, 3> forward{0.0F, 0.0F, 1.0F};
    float axial_far_distance{1000.0F};
    float lod_bias_squared{4.0F};
    float lod_scale_squared{1.0F};
    float facing_max_distance_squared{90000.0F};
    float facing_mode_2_min_distance_squared{0.01F};
};

struct TrackGraphicsFacingTransform {
    std::uint32_t node_offset{};
    // Three native basis vectors followed by the model center. These are
    // column vectors in the model-to-world transform used by the renderer.
    std::array<float, 12> basis_and_position{};
};

struct TrackGraphicsNodeSelection {
    std::size_t visited_node_count{};
    std::array<std::size_t, 8> visited_type_counts{};
    std::vector<std::uint32_t> ordinary_node_offsets;
    std::vector<std::uint32_t> facing_mode_1_node_offsets;
    std::vector<std::uint32_t> facing_mode_2_node_offsets;
    std::vector<std::uint32_t> registered_mode_4_node_offsets;
    std::vector<TrackGraphicsFacingTransform> facing_mode_1_transforms;
    std::vector<TrackGraphicsFacingTransform> facing_mode_2_transforms;
    // Static placement inference for referenced mode-4 models: identity
    // orientation at the GRAF center. Live WOBL snapshots can replace these
    // per frame once physics task dispatch is connected and compared.
    std::vector<TrackGraphicsFacingTransform> registered_mode_4_transforms;
    bool traversal_complete{};
};

[[nodiscard]] std::optional<TrackGraphicsResourcePayload>
parse_track_graphics_resource(std::uint32_t resource_version,
                              std::span<const std::byte> payload);

[[nodiscard]] TrackGraphicsInventory summarize_track_graphics(
    const TrackGraphicsResourcePayload& resource) noexcept;

// Reconstructs 0x00467770's signed ID bounds check and first-entry lookup.
// The returned node offset stands in for the native process-local model handle.
[[nodiscard]] std::optional<std::uint32_t> lookup_registered_facing_node(
    const TrackGraphicsResourcePayload& resource, std::int32_t facing_id) noexcept;

[[nodiscard]] TrackGraphicsNodeSelection select_track_graphics_nodes(
    const TrackGraphicsResourcePayload& resource,
    const TrackGraphicsCamera& camera);

} // namespace viper
