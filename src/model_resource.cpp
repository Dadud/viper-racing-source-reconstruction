#include "viper/model_resource.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <utility>

namespace viper {
namespace {

constexpr std::array<std::size_t, 5> kSectionStrides{32U, 32U, 8U, 16U, 4U};

std::uint16_t read_u16(const std::span<const std::byte> bytes,
                       const std::size_t offset) noexcept
{
    return static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset])) |
        static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset + 1U]) << 8U);
}

std::uint32_t read_u32(const std::span<const std::byte> bytes,
                       const std::size_t offset) noexcept
{
    return static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset])) |
        (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1U])) << 8U) |
        (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2U])) << 16U) |
        (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3U])) << 24U);
}

std::optional<std::size_t> section_bytes(const std::uint32_t count,
                                         const std::size_t stride) noexcept
{
    const auto value = static_cast<std::size_t>(count);
    if (value > std::numeric_limits<std::size_t>::max() / stride) return std::nullopt;
    return value * stride;
}

} // namespace

std::optional<ModelResource> parse_model_resource(
    const std::uint32_t resource_version,
    const std::span<const std::byte> payload)
{
    using Model = ModelResource;
    if ((resource_version != 0U && resource_version != 1U) ||
        payload.size() < Model::header_size) return std::nullopt;

    ModelResource result{};
    result.resource_version = 1U; // Native loader normalizes accepted v0 data to v1.
    std::size_t cursor = Model::header_size;
    for (std::size_t section = 0; section < result.section_counts.size(); ++section) {
        result.section_counts[section] = read_u32(payload, section * 8U);
        const auto bytes = section_bytes(result.section_counts[section], kSectionStrides[section]);
        if (!bytes || cursor > payload.size() || *bytes > payload.size() - cursor)
            return std::nullopt;
        cursor += *bytes;
    }
    // The shipped serializer emits exactly the five counted sections. Reject
    // unaccounted data instead of guessing how it should affect the layout.
    if (cursor != payload.size()) return std::nullopt;

    const auto vertex_count = static_cast<std::size_t>(result.section_counts[0]);
    const auto group_count = static_cast<std::size_t>(result.section_counts[1]);
    const auto triangle_count = static_cast<std::size_t>(result.section_counts[2]);
    std::size_t vertices_offset = Model::header_size;
    const auto vertices_bytes = *section_bytes(result.section_counts[0], kSectionStrides[0]);
    const auto groups_offset = vertices_offset + vertices_bytes;
    const auto groups_bytes = *section_bytes(result.section_counts[1], kSectionStrides[1]);
    const auto triangles_offset = groups_offset + groups_bytes;

    result.vertices.reserve(vertex_count);
    for (std::size_t index = 0; index < vertex_count; ++index) {
        ModelVertex vertex{};
        const auto base = vertices_offset + index * kSectionStrides[0];
        for (std::size_t field = 0; field < vertex.fields.size(); ++field) {
            vertex.fields[field] = std::bit_cast<float>(read_u32(payload, base + field * 4U));
        }
        if (!std::isfinite(vertex.fields[0]) || !std::isfinite(vertex.fields[1]) ||
            !std::isfinite(vertex.fields[2])) return std::nullopt;
        result.vertices.push_back(vertex);
    }

    result.material_groups.reserve(group_count);
    for (std::size_t index = 0; index < group_count; ++index) {
        const auto base = groups_offset + index * kSectionStrides[1];
        ModelMaterialGroup group{};
        std::array<char, 16> name{};
        for (std::size_t i = 0; i < name.size(); ++i)
            name[i] = static_cast<char>(std::to_integer<unsigned char>(payload[base + i]));
        if (resource_version == 0U) {
            // Observed legacy conversion clears the high four name/metadata bytes.
            std::fill(name.begin() + 12, name.end(), '\0');
        }
        const auto end = std::find(name.begin(), name.end(), '\0');
        group.material_name.assign(name.begin(), end);
        group.first_vertex = read_u16(payload, base + 0x18U);
        group.end_vertex = read_u16(payload, base + 0x1aU);
        group.first_triangle = read_u16(payload, base + 0x1cU);
        group.end_triangle = read_u16(payload, base + 0x1eU);
        if (group.first_vertex > group.end_vertex || group.end_vertex > vertex_count ||
            group.first_triangle > group.end_triangle ||
            group.end_triangle > triangle_count) return std::nullopt;
        const auto index_count = static_cast<std::size_t>(group.end_triangle - group.first_triangle) * 3U;
        group.local_indices.reserve(index_count);
        for (std::size_t triangle = group.first_triangle; triangle < group.end_triangle; ++triangle) {
            const auto triangle_base = triangles_offset + triangle * kSectionStrides[2];
            for (std::size_t corner = 0; corner < 3U; ++corner) {
                const auto source_index = read_u16(payload, triangle_base + corner * 2U);
                if (source_index < group.first_vertex || source_index >= group.end_vertex)
                    return std::nullopt;
                group.local_indices.push_back(static_cast<std::uint16_t>(source_index - group.first_vertex));
            }
        }
        result.material_groups.push_back(std::move(group));
    }
    return result;
}

} // namespace viper
