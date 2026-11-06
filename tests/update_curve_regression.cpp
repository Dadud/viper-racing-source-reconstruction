#include "viper/update_curve.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
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
        viper::UpdateCurveSetterPrefixInput setter{};
        setter.getter_value_0x10 = 500.0F;
        setter.control_value_0x2c = 0.5F;
        setter.derived_value_0x30 = 0.5F;
        setter.setter_argument = -3.0F;
        setter.curve.blend_value_0x38 = 0.2F;
        setter.curve.cutoff_0x3c = 1000.0F;
        setter.curve.linear_0x48 = 2.0F;
        setter.curve.intercept_0x4c = 10.0F;
        const auto prefix = viper::project_update_curve_setter_prefix(setter);
        require(prefix && std::fabs(prefix->blended_value_0x38 - 0.29F) < 1e-6F &&
                std::fabs(prefix->curve_scale - 0.75F) < 1e-6F &&
                std::fabs(prefix->raw_curve_output - 757.5F) < 1e-4F &&
                std::fabs(prefix->integrator_argument - 754.5F) < 1e-4F,
            "curve setter prefix must preserve smoothing, raw curve and integrator input");
        setter.getter_value_0x10 = 1000.0F;
        const auto cutoff = viper::project_update_curve_setter_prefix(setter);
        require(cutoff && cutoff->raw_curve_output == 0.0F &&
                cutoff->integrator_argument == -3.0F,
            "curve setter must suppress the curve at the cutoff");
        setter.curve.mode_0x41 = 1U;
        setter.curve.mode_0x40 = 1U;
        const auto alternate = viper::project_update_curve_setter_prefix(setter);
        require(alternate && alternate->raw_curve_output == 80.0F &&
                alternate->integrator_argument == 77.0F,
            "curve setter alternate mode must return the native fixed value");
        setter.getter_value_0x10 = std::numeric_limits<float>::quiet_NaN();
        require(!viper::project_update_curve_setter_prefix(setter),
            "curve setter must reject non-finite inputs");
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
        const auto curve_writes =
            viper::make_configured_update_curve_object_writes(configured, 0xbfcU);
        const auto find_curve_write = [&curve_writes](const std::uint32_t offset)
            -> const viper::ConfiguredUpdateCurveObjectWrite* {
            for (const auto& write : curve_writes)
                if (write.offset == offset) return &write;
            return nullptr;
        };
        const auto* getter_write = find_curve_write(0xc0cU);
        const auto* derived_write = find_curve_write(0xc2cU);
        const auto* getter_mode_write = find_curve_write(0xc3cU);
        require(getter_write != nullptr && getter_write->width == 4U &&
                std::bit_cast<float>(getter_write->value_bits) == 1000.0F,
            "0x4401D0 must initialize curve +0x10 to the native +0 getter value");
        require(derived_write != nullptr && derived_write->width == 4U &&
                std::bit_cast<float>(derived_write->value_bits) ==
                    configured.derived_value_0x30,
            "0x4401D0 derived value must land at curve +0x30, separate from getter +0x10");
        require(getter_mode_write != nullptr && getter_mode_write->width == 1U,
            "0x4401D0 mode writes must preserve their byte width");

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
