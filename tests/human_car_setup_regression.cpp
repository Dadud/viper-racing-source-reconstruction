#include "viper/vehicle_setup.hpp"

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
        std::vector<std::byte> file(viper::HumanCarSetupFile::file_size);
        for (std::size_t index = 0;
             index < viper::HumanCarSetupFile::active_setup_size; ++index) {
            file[index] = static_cast<std::byte>((index * 37U) & 0xffU);
        }
        write_u32_le(file, viper::HumanCarSetupFile::active_setup_size, 1U);

        const auto parsed = viper::parse_human_car_setup_file(file);
        require(parsed.has_value(), "an exact-size .csu file must be accepted");
        require(!parsed->version_warning && parsed->version == 1U,
            "version 1 must not produce the native warning state");
        require(std::equal(parsed->active_setup.begin(), parsed->active_setup.end(),
                file.begin()),
            "the first 0x8c bytes must form the active human setup record");

        write_u32_le(file, viper::HumanCarSetupFile::active_setup_size, 7U);
        const auto mismatched_version = viper::parse_human_car_setup_file(file);
        require(mismatched_version && mismatched_version->version_warning &&
                mismatched_version->version == 7U,
            "a non-1 version must warn without rejecting an exact-size file");

        file.pop_back();
        require(!viper::parse_human_car_setup_file(file),
            "a short .csu file must be rejected for fallback");
        file.push_back(std::byte{0});
        file.push_back(std::byte{0});
        require(!viper::parse_human_car_setup_file(file),
            "an oversized .csu file must be rejected for fallback");

        std::cout << "human car setup regression checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
