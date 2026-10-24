#include "viper/world_track_objects.hpp"

#include <algorithm>
#include <array>
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

        viper::CarFileResourcePayloads resources{};
        viper::CarFilePayload car_file{};
        car_file.resource_version = viper::CarFilePayload::supported_resource_version;
        car_file.raw_words.fill(0x3f800000U);
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
