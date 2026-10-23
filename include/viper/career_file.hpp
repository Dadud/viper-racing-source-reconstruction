#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace viper {

inline constexpr std::uint32_t career_file_magic = 0x43494e46U;
inline constexpr std::uint32_t career_file_version = 2U;
inline constexpr std::size_t career_file_payload_size = 0x674;
inline constexpr std::uint8_t career_file_xor_key = 0xa4U;

enum class CareerFileLoadStatus {
    open_failed,
    header_read_failed,
    invalid_header,
    payload_read_failed,
    inconsistent_payload_observation,
    checksum_mismatch,
    loaded,
};

struct CareerFileLoadObservation {
    bool file_open_succeeded{};
    bool header_read_succeeded{};
    std::array<std::uint32_t, 4> header_words{};
    bool payload_read_succeeded{};
    std::span<const std::uint8_t> encrypted_payload{};
};

struct CareerFileLoadResult {
    CareerFileLoadStatus status{CareerFileLoadStatus::open_failed};
    std::vector<std::uint8_t> decoded_payload;
    std::size_t requested_header_bytes{0x10};
    std::size_t requested_payload_bytes{career_file_payload_size};
    std::uint32_t computed_checksum{};
    bool header_read_reached{};
    bool payload_read_reached{};
    bool panic_helper_reached{};
    bool file_handle_close_reached{};
    bool header_error_logged{};
    bool corrupt_file_error_logged{};
    bool checksum_error_logged{};
    bool returns_success{};
};

struct CareerFileSaveImage {
    std::vector<std::uint8_t> file_bytes;
    std::uint32_t checksum{};

    [[nodiscard]] bool valid() const noexcept
    {
        return file_bytes.size() == 0x10 + career_file_payload_size;
    }
};

// 0x4AB4A0 formats "%scareer\\career%d.dat" from the Config directory and
// the selected career slot.
[[nodiscard]] std::string career_slot_path(
    std::string_view config_directory,
    int slot);

// Models 0x4ABE20's header check, XOR decode, checksum, and handle cleanup.
[[nodiscard]] CareerFileLoadResult load_career_file(
    const CareerFileLoadObservation& observation);

// Produces the exact little-endian header and XOR-encoded payload passed to
// the original file writer. This does not perform filesystem operations.
[[nodiscard]] CareerFileSaveImage make_career_file_save_image(
    std::span<const std::uint8_t> decoded_payload);

// Models the observed new-career initializer. The name span includes its NUL
// byte; the two settings are kept as raw payload fields until their encodings
// are independently established.
[[nodiscard]] std::optional<std::vector<std::uint8_t>>
make_new_career_payload(
    std::span<const std::uint8_t> player_name_with_terminator,
    std::uint32_t raw_setting_at_0x10,
    std::uint8_t raw_setting_at_0x14);

} // namespace viper
