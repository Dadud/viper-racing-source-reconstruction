#include "viper/replay_file.hpp"

#include <algorithm>
#include <fstream>
#include <limits>

namespace viper {
namespace {

constexpr std::size_t kRecordBytes = 0xcd4;
constexpr std::size_t kHeaderBytes = 0x1c;
constexpr std::uint32_t kCookie = 0x5245504c;
constexpr std::uint32_t kVersion = 6;
constexpr std::uint32_t kExpectedWordCount = 3647;
constexpr std::uint32_t kExpectedPacketSize = 575;
constexpr std::uint32_t kMaxPayloadBytes = 0x200000;

std::uint32_t read_u32(const std::span<const std::uint8_t> data,
                       const std::size_t offset)
{
    return static_cast<std::uint32_t>(data[offset]) |
        (static_cast<std::uint32_t>(data[offset + 1]) << 8U) |
        (static_cast<std::uint32_t>(data[offset + 2]) << 16U) |
        (static_cast<std::uint32_t>(data[offset + 3]) << 24U);
}

void append_u32(std::vector<std::uint8_t>& out, const std::uint32_t value)
{
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8U));
    out.push_back(static_cast<std::uint8_t>(value >> 16U));
    out.push_back(static_cast<std::uint8_t>(value >> 24U));
}

ReplayFileResult failure(std::string message)
{
    ReplayFileResult result;
    result.error = std::move(message);
    return result;
}

} // namespace

ReplayFileResult decode_replay_file(const std::span<const std::uint8_t> bytes)
{
    if (bytes.size() < kRecordBytes + kHeaderBytes) {
        return failure("replay is shorter than its fixed record and header");
    }
    const auto h = bytes.subspan(kRecordBytes, kHeaderBytes);
    if (read_u32(h, 0) != kCookie) return failure("unsupported replay cookie");
    if (read_u32(h, 4) != kVersion) return failure("unsupported replay version");
    const auto timestamp = static_cast<std::int32_t>(read_u32(h, 8));
    const auto frames = static_cast<std::int32_t>(read_u32(h, 12));
    const auto packet_size = read_u32(h, 16);
    const auto word_count = read_u32(h, 20);
    const auto payload_bytes = read_u32(h, 24);
    if (word_count != kExpectedWordCount) return failure("replay frame table has incompatible word count");
    if (packet_size != kExpectedPacketSize) return failure("replay packet size is incompatible");
    if (payload_bytes > kMaxPayloadBytes) return failure("replay payload exceeds 2 MiB limit");
    if (frames < 0 || static_cast<std::uint32_t>(frames) > word_count)
        return failure("replay frame count is out of range");
    const std::size_t table_bytes = static_cast<std::size_t>(word_count) * 4U;
    const std::size_t expected = kRecordBytes + kHeaderBytes + table_bytes + payload_bytes;
    if (bytes.size() != expected) return failure("replay length does not match its declared sections");

    ReplayFileResult result;
    std::copy_n(bytes.begin(), kRecordBytes, result.replay.engine_record.begin());
    result.replay.timestamp_or_index = timestamp;
    result.replay.frame_count = frames;
    result.replay.base_packet_size = packet_size;
    result.replay.frame_words.reserve(word_count);
    std::size_t cursor = kRecordBytes + kHeaderBytes;
    for (std::uint32_t i = 0; i < word_count; ++i, cursor += 4U)
        result.replay.frame_words.push_back(read_u32(bytes, cursor));
    result.replay.payload.assign(bytes.begin() + static_cast<std::ptrdiff_t>(cursor), bytes.end());
    return result;
}

std::vector<std::uint8_t> encode_replay_file(const ReplayFile& replay, std::string& error)
{
    error.clear();
    if (replay.frame_words.size() != kExpectedWordCount) {
        error = "replay frame table must contain exactly 3647 words";
        return {};
    }
    if (replay.base_packet_size != kExpectedPacketSize) {
        error = "replay packet size is incompatible";
        return {};
    }
    if (replay.frame_count < 0 || static_cast<std::size_t>(replay.frame_count) > replay.frame_words.size()) {
        error = "replay frame count is out of range";
        return {};
    }
    if (replay.payload.size() > kMaxPayloadBytes || replay.payload.size() > std::numeric_limits<std::uint32_t>::max()) {
        error = "replay payload exceeds supported size";
        return {};
    }
    std::vector<std::uint8_t> out;
    out.reserve(kRecordBytes + kHeaderBytes + replay.frame_words.size() * 4U + replay.payload.size());
    out.insert(out.end(), replay.engine_record.begin(), replay.engine_record.end());
    append_u32(out, kCookie); append_u32(out, kVersion);
    append_u32(out, static_cast<std::uint32_t>(replay.timestamp_or_index));
    append_u32(out, static_cast<std::uint32_t>(replay.frame_count));
    append_u32(out, replay.base_packet_size);
    append_u32(out, static_cast<std::uint32_t>(replay.frame_words.size()));
    append_u32(out, static_cast<std::uint32_t>(replay.payload.size()));
    for (const auto word : replay.frame_words) append_u32(out, word);
    out.insert(out.end(), replay.payload.begin(), replay.payload.end());
    return out;
}

ReplayFileResult read_replay_file(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) return failure("cannot open replay file");
    const auto end = input.tellg();
    if (end < 0 || static_cast<std::uint64_t>(end) > kRecordBytes + kHeaderBytes +
        static_cast<std::uint64_t>(kExpectedWordCount) * 4U + kMaxPayloadBytes)
        return failure("replay file size is invalid");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    input.seekg(0);
    if (!bytes.empty() && !input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
        return failure("cannot read replay file");
    return decode_replay_file(bytes);
}

bool write_replay_file(const std::filesystem::path& path, const ReplayFile& replay, std::string& error)
{
    const auto bytes = encode_replay_file(replay, error);
    if (!error.empty()) return false;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) { error = "cannot create replay file"; return false; }
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!output) { error = "cannot write replay file"; return false; }
    return true;
}

} // namespace viper
