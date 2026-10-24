#include "viper/world_track_objects.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

void write_u32_le(std::vector<std::byte>& bytes, const std::size_t offset,
                  const std::uint32_t value)
{
    for (std::size_t i = 0; i < 4; ++i)
        bytes[offset + i] = static_cast<std::byte>((value >> (i * 8U)) & 0xffU);
}

void require(const bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

} // namespace

int main()
{
    try {
        constexpr std::size_t header_size =
            viper::TrackObjectTextTable::header_size;
        constexpr std::uint32_t stride = 16;
        std::vector<std::byte> payload(header_size + 2U * stride);
        write_u32_le(payload, 0, 2);
        for (std::size_t offset = 0; offset < 16; ++offset)
            write_u32_le(payload, 8U + offset * 4U, 0);
        write_u32_le(payload, 0x48, stride);
        constexpr std::array<std::string_view, 2> names{"dundas", "bemidji"};
        for (std::size_t row = 0; row < names.size(); ++row)
            for (std::size_t column = 0; column < names[row].size(); ++column)
                payload[header_size + row * stride + column] =
                    static_cast<std::byte>(names[row][column]);

        const auto table = viper::parse_track_object_text_table(payload);
        require(table.has_value(), "valid STAB text table rejected");
        require(table->line_count == 2, "text table row count mismatch");
        require(viper::track_object_text_line(*table, 0, 0) == "dundas",
                "first selector row mismatch");
        require(viper::track_object_text_line(*table, 1, 0) == "bemidji",
                "second selector row mismatch");
        require(!viper::track_object_text_line(*table, 2, 0),
                "out-of-range selector row accepted");

        write_u32_le(payload, 0x48, 0);
        require(!viper::parse_track_object_text_table(payload),
                "zero-stride nonempty table accepted");
        std::cout << "track table regression passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
