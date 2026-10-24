#include "viper/career_file_store.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures{};

void check(const bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

} // namespace

int main()
{
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = std::filesystem::temp_directory_path() /
        ("viper-career-store-" + std::to_string(nonce));
    std::vector<std::uint8_t> payload(viper::career_file_payload_size);
    for (std::size_t i = 0; i < payload.size(); ++i)
        payload[i] = static_cast<std::uint8_t>((i * 37U + 11U) & 0xffU);

    const auto saved = viper::save_career_slot(root, 2, payload);
    if (!saved.saved) std::cerr << "save error: " << saved.error << '\n';
    check(saved.saved, "writes slot through native encoded file image");
    check(saved.path == root / "career" / "career2.dat",
        "uses the native Config/career/careerN.dat path");

    const auto loaded = viper::load_career_slot(root, 2);
    if (!loaded.file.returns_success) std::cerr << "load error: " << loaded.error << '\n';
    check(loaded.file.status == viper::CareerFileLoadStatus::loaded,
        "loads and validates the saved native slot");
    check(loaded.file.decoded_payload == payload,
        "round-trips all decoded payload bytes");

    auto corrupt_path = saved.path;
    {
        std::fstream file(corrupt_path, std::ios::binary | std::ios::in | std::ios::out);
        file.seekg(0x10 + 0x200);
        char byte{};
        file.read(&byte, 1);
        byte = static_cast<char>(static_cast<unsigned char>(byte) ^ 0x01U);
        file.seekp(0x10 + 0x200);
        file.write(&byte, 1);
    }
    const auto corrupt_load = viper::load_career_slot(root, 2);
    check(corrupt_load.file.status == viper::CareerFileLoadStatus::checksum_mismatch,
        "rejects payload tampering through the native checksum");

    const auto replaced = viper::save_career_slot(root, 2, payload);
    check(replaced.saved, "replaces an existing slot after writing a complete image");

    const auto invalid = viper::save_career_slot(root, 3,
        std::span<const std::uint8_t>(payload.data(), payload.size() - 1U));
    check(!invalid.saved, "rejects payloads with non-native size");
    const auto negative = viper::load_career_slot(root, -1);
    check(!negative.error.empty(), "rejects negative slot numbers");

    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    return failures == 0 ? 0 : 1;
}
