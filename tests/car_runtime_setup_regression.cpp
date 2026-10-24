#include "viper/world_track_objects.hpp"
#include "viper/wheel_body_coupling.hpp"
#include "viper/ncar_state.hpp"
#include "viper/model_resource.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
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

void write_u32_le(std::vector<std::byte>& bytes, const std::size_t offset,
                  const std::uint32_t value)
{
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        bytes[offset + index] = static_cast<std::byte>(
            (value >> (index * 8U)) & 0xffU);
    }
}

} // namespace

int main()
{
    try {
        constexpr std::size_t setup_size =
            viper::CarFileRuntimeMappingObservation::configuration_word_count *
            sizeof(std::uint32_t);
        std::vector<std::byte> setup(setup_size);
        std::array<std::uint32_t,
            viper::CarFileRuntimeMappingObservation::configuration_word_count> words{};
        for (std::size_t index = 0; index < words.size(); ++index) {
            words[index] = 0x3f000000U + static_cast<std::uint32_t>(index);
            write_u32_le(setup, index * sizeof(std::uint32_t), words[index]);
        }
        words[0x22U] = std::bit_cast<std::uint32_t>(1.0F);
        write_u32_le(setup, 0x22U * sizeof(std::uint32_t), words[0x22U]);

        viper::CarFileResourcePayloads resources{};
        viper::CarFilePayload car_file{};
        car_file.resource_version = viper::CarFilePayload::supported_resource_version;
        car_file.raw_words.fill(0x3f800000U);
        car_file.raw_words[12U] = std::bit_cast<std::uint32_t>(200.0F);
        car_file.raw_words[13U] = std::bit_cast<std::uint32_t>(25.0F);
        car_file.raw_words[0x66U] = std::bit_cast<std::uint32_t>(10.0F);
        car_file.raw_words[0x67U] = std::bit_cast<std::uint32_t>(10.0F);
        // A zero setup-count discriminator selects the configured setup words.
        car_file.raw_words[0x3aU] = 0U;
        resources.car_file = car_file;

        const auto preparation =
            viper::prepare_car_file_runtime_mapping_from_setup_payload(
                resources, setup, {}, false, false);
        require(preparation.input_complete && preparation.mapping.has_value(),
            "a full 0x8c setup payload and CARF v6 must prepare a runtime mapping");
        const auto matching_write = std::find_if(
            preparation.mapping->writes.begin(), preparation.mapping->writes.end(),
            [](const viper::CarFileRuntimeFieldWrite& write) {
                return write.byte_offset == 0x94U;
            });
        require(matching_write != preparation.mapping->writes.end() &&
                matching_write->raw_word == words[0x14U],
            "setup source field 0x14 must reach the mapped vehicle field");

        std::vector<std::byte> runtime_record(
            viper::CarFileRuntimeMapping::runtime_record_size);
        require(viper::apply_car_file_runtime_mapping(
                    runtime_record, *preparation.mapping),
            "prepared CARF mapping must apply to the bounded runtime record");
        const auto body_shift_write = std::find_if(
            preparation.mapping->writes.begin(), preparation.mapping->writes.end(),
            [](const viper::CarFileRuntimeFieldWrite& write) {
                return write.byte_offset == 0x128U;
            });
        require(body_shift_write != preparation.mapping->writes.end() &&
                body_shift_write->raw_word == car_file.raw_words[0x66U],
            "CARF v6 word 0x66 must map directly to runtime record +0x128");
        const auto mapped_body_shift = [&runtime_record] {
            std::uint32_t raw{};
            for (std::size_t byte_index = 0; byte_index < sizeof(raw); ++byte_index) {
                raw |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(
                    runtime_record[0x128U + byte_index])) << (byte_index * 8U);
            }
            return std::bit_cast<float>(raw);
        }();
        require(mapped_body_shift == 10.0F,
            "mapped runtime record must retain the native +0x128 body-shift scalar");
        viper::ModelVertexBounds model_bounds{};
        model_bounds.minimum = {-1.0F, -1.0F, -1.0F};
        model_bounds.maximum = {1.0F, 1.0F, 3.0F};
        model_bounds.vertex_count = 2U;
        viper::NcarResetSphereBoundsObservation sphere_observation{};
        sphere_observation.extents_pointer_present = true;
        sphere_observation.precomputed_model_bounds = model_bounds;
        sphere_observation.car_record_0x58 = 200.0F;
        sphere_observation.car_record_0x5c = 25.0F;
        sphere_observation.car_record_0x128 = mapped_body_shift;
        const auto sphere_geometry =
            viper::derive_ncar_reset_sphere_geometry(sphere_observation);
        require(sphere_geometry.inputs_complete &&
                std::fabs(sphere_geometry.local_geometry.local_b0_after_body_shift -
                    (-1.0F - 10.0F * 0.0254F)) < 1.0e-5F &&
                std::fabs(sphere_geometry.local_geometry.local_bc_after_clamp - 1.0F) <
                    1.0e-5F,
            "the CARF-derived runtime +0x128 value must reach native proxy bounds");
        const auto callback_setup =
            viper::initialize_ncar_integrator_wheel_callback_setup_from_runtime_record(
                runtime_record);
        require(callback_setup.has_value() &&
                std::fabs(callback_setup->value_0xd84 - 3.81F) < 1.0e-5F &&
                std::fabs(callback_setup->value_0xd90 + 1.27F) < 1.0e-5F &&
                std::fabs(callback_setup->angle_0xdcc - 0.174F) < 1.0e-5F,
            "mapped CARF width/ratio/value must reach the exact constructor callback fields");
        require(!viper::initialize_ncar_integrator_wheel_callback_setup_from_runtime_record(
                    std::span<const std::byte>(runtime_record).first(0x12fU)),
            "runtime records missing bytes of +0x12C must be rejected");

        setup.pop_back();
        const auto short_preparation =
            viper::prepare_car_file_runtime_mapping_from_setup_payload(
                resources, setup, {}, false, false);
        require(!short_preparation.input_complete && !short_preparation.mapping,
            "a short CCS/CSU setup buffer must not produce a partial mapping");

        std::cout << "car runtime setup regression checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
