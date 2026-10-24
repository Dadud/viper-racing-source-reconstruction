#include "viper/career_file_store.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

namespace viper {
namespace {

std::uint32_t read_u32_le(const std::uint8_t* bytes) noexcept
{
    return static_cast<std::uint32_t>(bytes[0]) |
        (static_cast<std::uint32_t>(bytes[1]) << 8U) |
        (static_cast<std::uint32_t>(bytes[2]) << 16U) |
        (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

std::filesystem::path slot_path(const std::filesystem::path& config_directory,
                                const int slot)
{
    if (slot < 0) throw std::invalid_argument("career slot must be nonnegative");
    return config_directory / "career" /
        ("career" + std::to_string(slot) + ".dat");
}

bool replace_file(const std::filesystem::path& from,
                  const std::filesystem::path& to,
                  std::error_code& error)
{
#ifdef _WIN32
    if (MoveFileExW(from.c_str(), to.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error.clear();
        return true;
    }
    error = std::error_code(static_cast<int>(GetLastError()),
        std::system_category());
    return false;
#else
    std::filesystem::rename(from, to, error);
    return !error;
#endif
}

} // namespace

CareerSlotLoadResult load_career_slot(
    const std::filesystem::path& config_directory, const int slot)
{
    CareerSlotLoadResult result{};
    try {
        result.path = slot_path(config_directory, slot);
    } catch (const std::exception& error) {
        result.error = error.what();
        return result;
    }

    std::ifstream input(result.path, std::ios::binary);
    if (!input) {
        result.file = load_career_file({});
        result.error = "could not open career slot: " + result.path.string();
        return result;
    }

    std::array<std::uint8_t, 0x10> header{};
    input.read(reinterpret_cast<char*>(header.data()),
        static_cast<std::streamsize>(header.size()));
    const bool header_read = input.gcount() ==
        static_cast<std::streamsize>(header.size());
    std::array<std::uint32_t, 4> header_words{};
    if (header_read) {
        for (std::size_t word = 0; word < header_words.size(); ++word)
            header_words[word] = read_u32_le(header.data() + word * 4U);
    }

    std::vector<std::uint8_t> encrypted(career_file_payload_size);
    bool payload_read = false;
    if (header_read && header_words[0] == career_file_magic &&
        header_words[1] == career_file_version &&
        header_words[2] == career_file_payload_size) {
        input.read(reinterpret_cast<char*>(encrypted.data()),
            static_cast<std::streamsize>(encrypted.size()));
        payload_read = input.gcount() ==
            static_cast<std::streamsize>(encrypted.size());
    }

    CareerFileLoadObservation observation{};
    observation.file_open_succeeded = true;
    observation.header_read_succeeded = header_read;
    observation.header_words = header_words;
    observation.payload_read_succeeded = payload_read;
    observation.encrypted_payload = encrypted;
    result.file = load_career_file(observation);
    if (!result.file.returns_success)
        result.error = "career slot failed native format validation: " +
            result.path.string();
    return result;
}

CareerSlotSaveResult save_career_slot(
    const std::filesystem::path& config_directory, const int slot,
    const std::span<const std::uint8_t> decoded_payload)
{
    CareerSlotSaveResult result{};
    try {
        result.path = slot_path(config_directory, slot);
    } catch (const std::exception& error) {
        result.error = error.what();
        return result;
    }

    const auto image = make_career_file_save_image(decoded_payload);
    if (!image.valid()) {
        result.error = "career payload has an invalid native size";
        return result;
    }

    std::error_code error;
    std::filesystem::create_directories(result.path.parent_path(), error);
    if (error) {
        result.error = "could not create career directory: " + error.message();
        return result;
    }

    static std::atomic<std::uint64_t> sequence{};
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    auto temporary = result.path;
    temporary += ".tmp-" + std::to_string(nonce) + "-" +
        std::to_string(sequence.fetch_add(1));
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            result.error = "could not create temporary career file: " + temporary.string();
            return result;
        }
        output.write(reinterpret_cast<const char*>(image.file_bytes.data()),
            static_cast<std::streamsize>(image.file_bytes.size()));
        output.flush();
        if (!output) {
            result.error = "failed while writing temporary career file: " + temporary.string();
            output.close();
            std::filesystem::remove(temporary, error);
            return result;
        }
        output.close();
        if (!output) {
            result.error = "failed while closing temporary career file: " + temporary.string();
            std::filesystem::remove(temporary, error);
            return result;
        }
    }

    if (!replace_file(temporary, result.path, error)) {
        result.error = "could not replace career slot: " + error.message();
        std::filesystem::remove(temporary, error);
        return result;
    }
    result.saved = true;
    return result;
}

CareerSlotDeleteResult delete_career_slot(
    const std::filesystem::path& config_directory, const int slot)
{
    CareerSlotDeleteResult result{};
    try {
        result.path = slot_path(config_directory, slot);
    } catch (const std::exception& error) {
        result.error = error.what();
        return result;
    }

    std::error_code error;
    result.deleted = std::filesystem::remove(result.path, error);
    if (error) {
        result.error = "could not delete career slot: " + error.message();
        return result;
    }
    result.already_missing = !result.deleted;
    return result;
}

} // namespace viper
