#include "viper/replay_file.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void require(const bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

std::vector<std::uint8_t> read_all(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void write_u32(std::vector<std::uint8_t>& bytes, const std::size_t offset,
               const std::uint32_t value)
{
    for (std::size_t i = 0; i < 4; ++i)
        bytes[offset + i] = static_cast<std::uint8_t>(value >> (8U * i));
}

} // namespace

int main()
try {
    viper::ReplayFile sample;
    sample.engine_record[0] = 3U;
    sample.engine_record[4] = 0xd4U;
    sample.engine_record[5] = 0x0cU;
    sample.timestamp_or_index = 17;
    sample.frame_count = 2;
    sample.frame_words.resize(3647);
    sample.frame_words[0] = 0x12345678U;
    sample.payload = {0, 1, 2, 0xff};
    std::string error;
    const auto encoded = viper::encode_replay_file(sample, error);
    require(error.empty() && !encoded.empty(), "valid replay should encode");
    const auto decoded = viper::decode_replay_file(encoded);
    require(static_cast<bool>(decoded), "encoded replay should decode");
    require(decoded.replay.timestamp_or_index == sample.timestamp_or_index &&
            decoded.replay.frame_count == sample.frame_count &&
            decoded.replay.frame_words == sample.frame_words &&
            decoded.replay.payload == sample.payload,
            "replay fields should round-trip");
    require(viper::encode_replay_file(decoded.replay, error) == encoded,
            "replay encoding should be byte stable");

    auto non_native_record = sample.engine_record;
    non_native_record[0] = 4U;
    require(!viper::validate_replay_engine_record(non_native_record, error) &&
            !error.empty(), "native record validator should reject wrong record type");
    non_native_record = sample.engine_record;
    non_native_record[4] = 0xd3U;
    require(!viper::validate_replay_engine_record(non_native_record, error) &&
            !error.empty(), "native record validator should reject wrong record size");
    require(viper::validate_replay_engine_record(sample.engine_record, error) &&
            error.empty(), "native record validator should accept type 3 and size 0xCD4");

    // The structural codec remains lossless for records that the native
    // replay loader would refuse to install.
    auto non_native_container = encoded;
    non_native_container[0] = 4U;
    const auto decoded_non_native = viper::decode_replay_file(non_native_container);
    require(static_cast<bool>(decoded_non_native) &&
            viper::encode_replay_file(decoded_non_native.replay, error) ==
                non_native_container,
            "container codec should preserve a non-native fixed record losslessly");

    // Exercise the actual save/reload path as well as the in-memory codec.
    // Distinct bytes in the opaque fixed record make accidental omission or
    // reinitialization visible in the file round-trip.
    for (std::size_t i = 0; i < sample.engine_record.size(); ++i)
        sample.engine_record[i] = static_cast<std::uint8_t>(i * 37U + 11U);
    sample.engine_record[0] = 3U;
    sample.engine_record[1] = 0U;
    sample.engine_record[2] = 0U;
    sample.engine_record[3] = 0U;
    sample.engine_record[4] = 0xd4U;
    sample.engine_record[5] = 0x0cU;
    sample.engine_record[6] = 0U;
    sample.engine_record[7] = 0U;
    const auto file_path = std::filesystem::temp_directory_path() /
        "viper_replay_file_regression.rpl";
    require(viper::write_replay_file(file_path, sample, error),
            "valid replay should save to disk");
    const auto reloaded = viper::read_replay_file(file_path);
    std::error_code remove_error;
    std::filesystem::remove(file_path, remove_error);
    require(static_cast<bool>(reloaded), "saved replay should reload");
    require(reloaded.replay.engine_record == sample.engine_record &&
            reloaded.replay.timestamp_or_index == sample.timestamp_or_index &&
            reloaded.replay.frame_count == sample.frame_count &&
            reloaded.replay.frame_words == sample.frame_words &&
            reloaded.replay.payload == sample.payload,
            "save/reload should preserve the complete native container");

    auto invalid_native_save = sample;
    invalid_native_save.engine_record[0] = 4U;
    const auto invalid_file_path = std::filesystem::temp_directory_path() /
        "viper_replay_file_invalid_record_regression.rpl";
    require(!viper::write_replay_file(invalid_file_path, invalid_native_save, error) &&
            !error.empty() && !std::filesystem::exists(invalid_file_path),
            "native save should reject an invalid fixed record before creating a file");
    {
        std::ofstream invalid_output(invalid_file_path, std::ios::binary);
        invalid_output.write(reinterpret_cast<const char*>(
            non_native_container.data()),
            static_cast<std::streamsize>(non_native_container.size()));
    }
    const auto invalid_native_load = viper::read_replay_file(invalid_file_path);
    std::filesystem::remove(invalid_file_path, remove_error);
    require(!invalid_native_load &&
            invalid_native_load.error.find("incompatible type or size") !=
                std::string::npos,
            "native load should reject a structurally valid replay with a bad fixed record");

    auto signed_header = sample;
    signed_header.timestamp_or_index = -1;
    const auto signed_bytes = viper::encode_replay_file(signed_header, error);
    const auto signed_decoded = viper::decode_replay_file(signed_bytes);
    require(static_cast<bool>(signed_decoded) &&
            signed_decoded.replay.timestamp_or_index == -1,
            "signed header fields should preserve their bit patterns");

    auto truncated = encoded;
    truncated.pop_back();
    require(!viper::decode_replay_file(truncated), "truncated replay should fail closed");
    auto wrong_version = encoded;
    wrong_version[0xcd4 + 4] = 5;
    require(!viper::decode_replay_file(wrong_version), "unsupported version should fail closed");

    constexpr std::size_t header = 0xcd4;
    const std::array<std::pair<std::size_t, std::uint32_t>, 6> bad_fields{{
        {header, 0U},                 // cookie
        {header + 16U, 576U},         // packet size
        {header + 20U, 3646U},        // runtime word count
        {header + 24U, 0x200001U},    // 2 MiB payload bound
        {header + 12U, 0xffffffffU},  // negative frame count
        {header + 12U, 3648U},        // frame count beyond word table
    }};
    for (const auto& [offset, value] : bad_fields) {
        auto malformed = encoded;
        write_u32(malformed, offset, value);
        require(!viper::decode_replay_file(malformed),
                "native header constraint should reject malformed replay");
    }

    auto extra_data = encoded;
    extra_data.push_back(0);
    require(!viper::decode_replay_file(extra_data),
            "undeclared trailing bytes should fail closed");

    auto invalid_encode = sample;
    invalid_encode.frame_count = 3648;
    require(viper::encode_replay_file(invalid_encode, error).empty() &&
            !error.empty(), "encoder should reject frame count beyond table");

#ifdef VIPER_BENCH_REPLAY_PATH
    const auto fixture_bytes = read_all(VIPER_BENCH_REPLAY_PATH);
    require(!fixture_bytes.empty(), "bench.rpl fixture should be readable");
    const auto fixture = viper::decode_replay_file(fixture_bytes);
    require(static_cast<bool>(fixture), "shipped bench.rpl should decode");
    require(viper::validate_replay_engine_record(
                fixture.replay.engine_record, error),
            "shipped bench.rpl should pass native fixed-record validation");
    require(viper::encode_replay_file(fixture.replay, error) == fixture_bytes,
            "shipped bench.rpl should round-trip losslessly");
#endif
    std::cout << "replay container regression passed\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << "replay container regression failure: " << error.what() << '\n';
    return 1;
}
