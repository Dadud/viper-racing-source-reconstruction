#include "viper/update_curve.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void require(const bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void write_u32(std::vector<std::byte>& bytes, const std::size_t offset,
               const std::uint32_t value)
{
    for (std::size_t index = 0U; index < 4U; ++index)
        bytes[offset + index] = static_cast<std::byte>(
            (value >> (index * 8U)) & 0xffU);
}

std::vector<std::byte> make_car_record(const std::uint32_t tag)
{
    std::vector<std::byte> bytes(0x1e4U);
    write_u32(bytes, 0U, tag);
    write_u32(bytes, 4U, static_cast<std::uint32_t>(bytes.size()));
    constexpr std::array<float, 8> values{
        300.0F, 450.0F, 3000.0F, 5000.0F,
        1500.0F, 0.4F, 100.0F, 400.0F};
    for (std::size_t index = 0U; index < values.size(); ++index)
        write_u32(bytes, 0x68U + index * 4U,
            std::bit_cast<std::uint32_t>(values[index]));
    return bytes;
}

} // namespace

int main()
{
    try {
        auto acar_record = make_car_record(0x41434152U);
        const auto source = viper::read_car_curve_source_record(acar_record);
        require(source.has_value() && source->value_0x68 == 300.0F &&
                source->value_0x6c == 450.0F && source->value_0x70 == 3000.0F &&
                source->value_0x74 == 5000.0F && source->value_0x78 == 1500.0F &&
                source->value_0x7c == 0.4F && source->value_0x80 == 100.0F &&
                source->value_0x84 == 400.0F,
            "ACAR curve source reader must retain all eight exact mapped fields");
        const auto configured = viper::configure_acar_update_curve(0U, *source);
        require(configured.scoring.cutoff_0x3c == 0.4F &&
                configured.scoring.gate_value_0x34 == 1500.0F &&
                configured.scoring.offset_0x54 == 5000.0F &&
                configured.scoring.penalty_0x58 != 0.0F,
            "ACAR constructor curve configuration must consume the retained source fields");

        auto pcar_record = make_car_record(0x50434152U);
        require(viper::read_car_curve_source_record(pcar_record).has_value(),
            "shared PCAR constructor curve path must accept a valid PCAR record");
        write_u32(acar_record, 4U, 0x1e0U);
        require(!viper::read_car_curve_source_record(acar_record).has_value(),
            "curve source reader must reject a declared-size mismatch");
        write_u32(acar_record, 4U, 0x1e4U);
        write_u32(acar_record, 0U, 0x12345678U);
        require(!viper::read_car_curve_source_record(acar_record).has_value(),
            "curve source reader must reject an unrelated record type");
        require(!viper::read_car_curve_source_record(
                std::span<const std::byte>(pcar_record).first(0x1e3U)).has_value(),
            "curve source reader must reject a truncated runtime record");

        std::cout << "Mapped PCAR/ACAR curve sources validate and configure.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
