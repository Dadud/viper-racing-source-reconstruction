#include "viper/model_resource.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

void write_u16(std::vector<std::byte>& bytes, const std::size_t at, const std::uint16_t value)
{
    bytes[at] = static_cast<std::byte>(value & 0xffU);
    bytes[at + 1U] = static_cast<std::byte>(value >> 8U);
}

void write_u32(std::vector<std::byte>& bytes, const std::size_t at, const std::uint32_t value)
{
    for (std::size_t i = 0; i < 4U; ++i)
        bytes[at + i] = static_cast<std::byte>((value >> (i * 8U)) & 0xffU);
}

std::vector<std::byte> triangle_model()
{
    // Header + 3 vertices (32 bytes each) + one 32-byte group + one 8-byte triangle.
    std::vector<std::byte> bytes(40U + 96U + 32U + 8U + 16U + 4U);
    write_u32(bytes, 0U, 3U);
    write_u32(bytes, 8U, 1U);
    write_u32(bytes, 16U, 1U);
    write_u32(bytes, 24U, 1U);
    write_u32(bytes, 32U, 1U);
    constexpr std::array<std::array<float, 3>, 3> positions{{
        {{0.0F, 0.0F, 0.0F}}, {{1.0F, 0.0F, 0.0F}}, {{0.0F, 0.0F, 1.0F}}}};
    for (std::size_t i = 0; i < positions.size(); ++i) {
        for (std::size_t axis = 0; axis < positions[i].size(); ++axis)
            write_u32(bytes, 40U + i * 32U + axis * 4U,
                std::bit_cast<std::uint32_t>(positions[i][axis]));
    }
    constexpr std::string_view material = "VIPER.tex";
    for (std::size_t i = 0; i < material.size(); ++i)
        bytes[136U + i] = static_cast<std::byte>(material[i]);
    write_u16(bytes, 160U, 0U);
    write_u16(bytes, 162U, 3U);
    write_u16(bytes, 164U, 0U);
    write_u16(bytes, 166U, 1U);
    write_u16(bytes, 168U, 0U);
    write_u16(bytes, 170U, 1U);
    write_u16(bytes, 172U, 2U);
    return bytes;
}

void check(const bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}

} // namespace

int main()
{
    try {
        auto bytes = triangle_model();
        const auto parsed = viper::parse_model_resource(1U, bytes);
        check(parsed.has_value(), "valid synthetic FNIM payload did not parse");
        check(parsed->vertices.size() == 3U, "vertex count mismatch");
        check(parsed->material_groups.size() == 1U, "material group count mismatch");
        const auto& group = parsed->material_groups.front();
        check(group.material_name == "VIPER.tex", "material name mismatch");
        check(group.local_indices == std::vector<std::uint16_t>{0U, 1U, 2U},
            "global source indices were not rebased to the group's vertex slice");
        const auto legacy_version = viper::parse_model_resource(0U, bytes);
        check(legacy_version && legacy_version->resource_version == 1U,
            "supported legacy FNIM version was not normalized");

        check(!viper::parse_model_resource(2U, bytes), "unsupported FNIM version accepted");
        bytes.pop_back();
        check(!viper::parse_model_resource(1U, bytes), "truncated FNIM payload accepted");
        bytes = triangle_model();
        write_u16(bytes, 168U, 3U);
        check(!viper::parse_model_resource(1U, bytes), "out-of-range group index accepted");
        bytes = triangle_model();
        bytes.push_back(std::byte{0});
        check(!viper::parse_model_resource(1U, bytes), "unaccounted FNIM tail accepted");
        std::cout << "FNIM parser regression checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
