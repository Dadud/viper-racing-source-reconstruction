#include "viper/career_file.hpp"

namespace viper {

namespace {

void append_u32_little_endian(std::vector<std::uint8_t>& bytes,
                              const std::uint32_t value)
{
    for (unsigned shift = 0; shift < 32; shift += 8) {
        bytes.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

void store_u32_little_endian(std::vector<std::uint8_t>& bytes,
                             const std::size_t offset,
                             const std::uint32_t value)
{
    for (unsigned shift = 0; shift < 32; shift += 8) {
        bytes[offset + shift / 8] =
            static_cast<std::uint8_t>(value >> shift);
    }
}

} // namespace

std::string career_slot_path(const std::string_view config_directory,
                             const int slot)
{
    return std::string(config_directory) + "career\\career" +
        std::to_string(slot) + ".dat";
}

CareerFileLoadResult load_career_file(
    const CareerFileLoadObservation& observation)
{
    CareerFileLoadResult result{};
    result.file_handle_close_reached = observation.file_open_succeeded;
    if (!observation.file_open_succeeded) {
        result.status = CareerFileLoadStatus::open_failed;
        return result;
    }

    result.header_read_reached = true;
    if (!observation.header_read_succeeded) {
        result.status = CareerFileLoadStatus::header_read_failed;
        result.panic_helper_reached = true;
        result.header_error_logged = true;
        return result;
    }

    if (observation.header_words[0] != career_file_magic ||
        observation.header_words[1] != career_file_version ||
        observation.header_words[2] != career_file_payload_size) {
        result.status = CareerFileLoadStatus::invalid_header;
        result.corrupt_file_error_logged = true;
        return result;
    }

    result.payload_read_reached = true;
    if (!observation.payload_read_succeeded) {
        result.status = CareerFileLoadStatus::payload_read_failed;
        result.panic_helper_reached = true;
        return result;
    }
    if (observation.encrypted_payload.size() != career_file_payload_size) {
        result.status = CareerFileLoadStatus::inconsistent_payload_observation;
        return result;
    }

    result.decoded_payload.reserve(career_file_payload_size);
    for (const auto encrypted_byte : observation.encrypted_payload) {
        const auto decoded_byte = static_cast<std::uint8_t>(
            encrypted_byte ^ career_file_xor_key);
        result.decoded_payload.push_back(decoded_byte);
        result.computed_checksum += decoded_byte;
    }

    if (result.computed_checksum != observation.header_words[3]) {
        result.status = CareerFileLoadStatus::checksum_mismatch;
        result.checksum_error_logged = true;
        return result;
    }

    result.status = CareerFileLoadStatus::loaded;
    result.returns_success = true;
    return result;
}

CareerFileSaveImage make_career_file_save_image(
    const std::span<const std::uint8_t> decoded_payload)
{
    CareerFileSaveImage image{};
    if (decoded_payload.size() != career_file_payload_size) {
        return image;
    }

    image.file_bytes.reserve(0x10 + career_file_payload_size);
    append_u32_little_endian(image.file_bytes, career_file_magic);
    append_u32_little_endian(image.file_bytes, career_file_version);
    append_u32_little_endian(
        image.file_bytes,
        static_cast<std::uint32_t>(career_file_payload_size));

    for (const auto byte : decoded_payload) {
        image.checksum += byte;
    }
    append_u32_little_endian(image.file_bytes, image.checksum);

    for (const auto byte : decoded_payload) {
        image.file_bytes.push_back(
            static_cast<std::uint8_t>(byte ^ career_file_xor_key));
    }
    return image;
}

std::optional<std::vector<std::uint8_t>> make_new_career_payload(
    const std::span<const std::uint8_t> player_name_with_terminator,
    const std::uint32_t raw_setting_at_0x10,
    const std::uint8_t raw_setting_at_0x14)
{
    if (player_name_with_terminator.empty() ||
        player_name_with_terminator.size() > career_file_payload_size ||
        player_name_with_terminator.back() != 0) {
        return std::nullopt;
    }

    std::vector<std::uint8_t> payload(career_file_payload_size, 0);
    for (std::size_t i = 0; i < player_name_with_terminator.size(); ++i) {
        payload[i] = player_name_with_terminator[i];
    }

    store_u32_little_endian(payload, 0x10, raw_setting_at_0x10);
    payload[0x14] = raw_setting_at_0x14;
    payload[0x1c] = 1;
    for (std::uint32_t index = 0; index < 8; ++index) {
        store_u32_little_endian(payload, 0x170 + index * 0x88, index);
    }
    return payload;
}

} // namespace viper
