#include "viper/replay_file.hpp"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
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

} // namespace

int main()
try {
    viper::ReplayFile sample;
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

    auto truncated = encoded;
    truncated.pop_back();
    require(!viper::decode_replay_file(truncated), "truncated replay should fail closed");
    auto wrong_version = encoded;
    wrong_version[0xcd4 + 4] = 5;
    require(!viper::decode_replay_file(wrong_version), "unsupported version should fail closed");

#ifdef VIPER_BENCH_REPLAY_PATH
    const auto fixture_bytes = read_all(VIPER_BENCH_REPLAY_PATH);
    require(!fixture_bytes.empty(), "bench.rpl fixture should be readable");
    const auto fixture = viper::decode_replay_file(fixture_bytes);
    require(static_cast<bool>(fixture), "shipped bench.rpl should decode");
    require(viper::encode_replay_file(fixture.replay, error) == fixture_bytes,
            "shipped bench.rpl should round-trip losslessly");
#endif
    std::cout << "replay container regression passed\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << "replay container regression failure: " << error.what() << '\n';
    return 1;
}
