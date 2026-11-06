#include "viper/resource_bundle.hpp"
#include "viper/wheel_body_coupling.hpp"
#include "viper/world_track_objects.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
std::uint32_t word(std::span<const std::byte> bytes, std::size_t offset) {
    std::uint32_t result{};
    for (std::size_t i = 0; i < 4; ++i)
        result |= std::to_integer<std::uint32_t>(bytes[offset + i]) << (8U * i);
    return result;
}
void write_word(std::span<std::byte> bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i)
        bytes[offset + i] = static_cast<std::byte>((value >> (8U * i)) & 0xffU);
}
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    try {
        const std::filesystem::path root(argv[1]);
        const auto common = viper::ResourceBundle::open(root / "common.res");
        for (const std::string car : {"viper", "sports", "sedan", "exotic", "plane"}) {
            const auto bundle = viper::ResourceBundle::open(root / (car + ".car"));
            const auto resources = viper::parse_car_file_resources(bundle, car, true);
            constexpr auto setup_type = viper::resource_type_code('C','C','S','0');
            auto setup = bundle.find_payload(car + ".ccs", setup_type);
            if (!setup) setup = common.find_payload("default.ccs", setup_type);
            require(resources.car_file.has_value() && setup.has_value(), "missing car or setup");
            const auto preparation = viper::prepare_car_file_runtime_mapping_from_setup_payload(
                resources, *setup, {}, false, false);
            require(preparation.input_complete && preparation.mapping.has_value(), "car mapping incomplete");
            std::vector<std::byte> record(viper::CarFileRuntimeMapping::runtime_record_size);
            require(viper::apply_car_file_runtime_mapping(record, *preparation.mapping), "car mapping failed");
            write_word(record, 0U, 0x50434152U);
            write_word(record, 4U, static_cast<std::uint32_t>(record.size()));
            const auto source = viper::read_car_curve_source_record(record);
            require(source.has_value(), "curve source missing");
            const auto curve = viper::configure_acar_update_curve(0U, *source, false);
            const auto signal = viper::make_ncar_integrator_signal_state_from_runtime_record(record);
            const auto front = viper::derive_ncar_wheel_scalar_0x5c(record, 0U);
            const auto rear = viper::derive_ncar_wheel_scalar_0x5c(record, 2U);
            require(signal && front && rear, "constructor signal missing");
            const auto multiplier = viper::derive_ncar_integrator_signal_multiplier(
                *front, *rear, std::bit_cast<float>(word(record, 0xb4U)),
                std::bit_cast<float>(word(record, 0x90U)), std::bit_cast<float>(0x3dd67750U));
            require(multiplier.has_value(), "invalid signal multiplier");
            viper::NcarBodyRuntimeObjectState body{};
            const auto defaults = viper::plan_ncar_constructor_subobject_defaults();
            require(viper::apply_ncar_body_object_writes(body, defaults.writes), "body defaults failed");
            for (const auto& field : viper::make_configured_update_curve_object_writes(curve, 0xbfcU)) {
                const viper::NcarObjectFieldWrite write{field.offset, field.width, field.value_bits};
                require(viper::apply_ncar_body_object_writes(body, std::span(&write, 1U)), "curve write failed");
            }
            std::vector<viper::NcarObjectFieldWrite> writes;
            const auto f = [&writes](std::uint32_t offset, float value) {
                writes.push_back({offset, 4U, std::bit_cast<std::uint32_t>(value)});
            };
            for (const auto offset : {0x50U,0x58U,0x258U,0x25cU,0x260U,0xc0cU,0xd70U,0xde0U}) f(offset, 0.0F);
            f(0x54U, 1.0F); f(0xdecU, 1.0F); f(0xc8cU, signal->limit_scale_0xc8c);
            f(0xc90U, signal->callback_scale_0xc90); f(0xca0U, std::bit_cast<float>(0x3e587910U));
            f(0xca4U, 1.0F / std::bit_cast<float>(0x3e587910U));
            const float profile = std::bit_cast<float>(word(record, 0xc8U));
            f(0xcacU, std::fabs(profile) > std::bit_cast<float>(0x34000000U)
                ? profile * std::bit_cast<float>(0x3facfe73U) : std::bit_cast<float>(0x3b03126fU));
            f(0xdd0U, *multiplier);
            writes.push_back({0xbe4U,1U,0U}); writes.push_back({0x534U,1U,0U});
            for (std::uint32_t i = 0; i < 7U; ++i) {
                const auto entry = word(record, 0x94U + i * 4U);
                if (!(std::bit_cast<float>(entry) > 0.01F)) break;
                writes.push_back({0xcc4U + i * 4U, 4U, entry});
            }
            require(viper::apply_ncar_body_object_writes(body, writes), "body setup failed");
            std::array<viper::NcarWheelRuntimeObjectState, 4> wheels{};
            for (std::size_t i = 0; i < wheels.size(); ++i) {
                const auto base = 0x554U + 0x1a4U * static_cast<std::uint32_t>(i);
                const viper::NcarObjectFieldWrite spin{base + 0x128U, 4U, 0U};
                require(viper::apply_ncar_wheel_object_writes(wheels[i], base, std::span(&spin, 1U)), "wheel init failed");
            }
            require(viper::read_ncar_body_object_u32(body, 0xcc4U).has_value(),
                "first selector value missing");
            for (int tick = 0; tick < 64; ++tick) {
                if (tick == 32) {
                    const std::array<viper::NcarObjectFieldWrite, 3> controls{{
                        {0xcbcU, 4U, 1U},
                        {0xde0U, 4U, std::bit_cast<std::uint32_t>(0.75F)},
                        {0x534U, 1U, 1U}}};
                    require(viper::apply_ncar_body_object_writes(body, controls), "control write failed");
                }
                const auto step = viper::advance_ncar_drivetrain_control_in_body_and_wheels(body, wheels, record, 0x554U);
                if (!step) throw std::runtime_error(car + " drivetrain step " + std::to_string(tick) + " failed");
                const auto getter = viper::read_ncar_body_object_u32(body, 0xc0cU);
                require(getter && std::isfinite(std::bit_cast<float>(*getter)), "invalid curve getter");
                const viper::NcarObjectFieldWrite sample{0xd70U,4U,*getter};
                require(viper::apply_ncar_body_object_writes(body, std::span(&sample, 1U)), "getter sample failed");
            }
            require(viper::read_ncar_body_object_u8(body, 0xcf0U) == 1U,
                "selected controller did not become active");
            std::cout << car << ": 64 finite drivetrain updates\n";
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
