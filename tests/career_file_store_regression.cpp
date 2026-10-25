#include "viper/career_file_store.hpp"
#include "viper/career_progression.hpp"

#include <array>
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
    std::array<float, viper::career_progression_row_count> profile_seconds{};
    profile_seconds.fill(100.0F);
    std::array<std::int32_t, viper::career_progression_row_count> random_samples{};
    random_samples.fill(100);
    const auto clamped_estimates = viper::estimate_career_entrant_order(
        profile_seconds, -200.0F, -1.0F, random_samples);
    for (const auto candidate_seconds : clamped_estimates.estimated_seconds) {
        check(candidate_seconds == 300.0F,
            "maps negative AI and player estimates to the observed 300-second fallback");
    }

    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = std::filesystem::temp_directory_path() /
        ("viper-career-store-" + std::to_string(nonce));
    const auto missing = viper::load_career_slot(root, 0);
    check(missing.file.status == viper::CareerFileLoadStatus::open_failed,
        "reports a missing slot as an open failure");
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

    const auto deleted = viper::delete_career_slot(root, 2);
    check(deleted.deleted && deleted.path == saved.path,
        "deletes only the selected native career slot");
    check(!std::filesystem::exists(saved.path),
        "removes the selected career file from disk");
    const auto deleted_load = viper::load_career_slot(root, 2);
    check(deleted_load.file.status == viper::CareerFileLoadStatus::open_failed,
        "deleted career slot reloads as empty");
    const auto missing_delete = viper::delete_career_slot(root, 2);
    check(missing_delete.already_missing && missing_delete.error.empty(),
        "deleting a missing slot is a safe no-op");
    const auto negative_delete = viper::delete_career_slot(root, -1);
    check(!negative_delete.deleted && !negative_delete.error.empty(),
        "rejects negative career slot numbers for deletion");

    const auto invalid = viper::save_career_slot(root, 3,
        std::span<const std::uint8_t>(payload.data(), payload.size() - 1U));
    check(!invalid.saved, "rejects payloads with non-native size");
    std::filesystem::create_directories(root / "career");
    const auto image = viper::make_career_file_save_image(payload);
    const auto truncated_path = root / "career" / "career4.dat";
    {
        std::ofstream file(truncated_path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(image.file_bytes.data()), 0x10 + 3);
    }
    const auto truncated = viper::load_career_slot(root, 4);
    check(truncated.file.status == viper::CareerFileLoadStatus::payload_read_failed,
        "reports a short encrypted payload as a payload read failure");

    const auto invalid_header_path = root / "career" / "career5.dat";
    {
        auto bad_header = image.file_bytes;
        bad_header[0] ^= 0x01U;
        std::ofstream file(invalid_header_path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(bad_header.data()), 0x10);
    }
    const auto invalid_header = viper::load_career_slot(root, 5);
    check(invalid_header.file.status == viper::CareerFileLoadStatus::invalid_header,
        "rejects a malformed native career header");

    const auto negative = viper::load_career_slot(root, -1);
    check(!negative.error.empty(), "rejects negative slot numbers");

    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    return failures == 0 ? 0 : 1;
}
