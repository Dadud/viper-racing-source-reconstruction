#include "viper/language_resource.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace viper {
namespace {

constexpr std::array<std::byte, 8> kLanguageMagic{
    std::byte{static_cast<unsigned char>('0')},
    std::byte{static_cast<unsigned char>('S')},
    std::byte{static_cast<unsigned char>('E')},
    std::byte{static_cast<unsigned char>('R')},
    std::byte{static_cast<unsigned char>('G')},
    std::byte{static_cast<unsigned char>('N')},
    std::byte{static_cast<unsigned char>('A')},
    std::byte{static_cast<unsigned char>('L')}};
constexpr std::size_t kObjectBase = 0x14U;
constexpr std::size_t kEntryCountOffset = 0x40U;
constexpr std::size_t kEntryTableOffset = 0x44U;
constexpr std::size_t kEntrySize = 8U;

std::optional<std::string> read_c_string(
    const std::span<const std::byte> bytes,
    const std::size_t offset)
{
    if (offset >= bytes.size()) {
        return std::nullopt;
    }
    const auto begin = bytes.begin() + static_cast<std::ptrdiff_t>(offset);
    const auto end = std::find(begin, bytes.end(), std::byte{0});
    if (end == bytes.end()) {
        return std::nullopt;
    }
    std::string value;
    value.reserve(static_cast<std::size_t>(end - begin));
    for (auto current = begin; current != end; ++current) {
        value.push_back(static_cast<char>(std::to_integer<unsigned char>(*current)));
    }
    return value;
}

std::optional<std::uint32_t> read_u32_le(
    const std::span<const std::byte> bytes,
    const std::size_t offset) noexcept
{
    if (offset > bytes.size() || bytes.size() - offset < sizeof(std::uint32_t)) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset]))
        | (static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 1U])) << 8U)
        | (static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 2U])) << 16U)
        | (static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 3U])) << 24U);
}

} // namespace

std::optional<std::string_view> LanguageResource::find(
    const std::string_view key) const noexcept
{
    const auto entry = std::find_if(entries.begin(), entries.end(),
        [key](const auto& candidate) { return candidate.key == key; });
    if (entry == entries.end()) {
        return std::nullopt;
    }
    return entry->value;
}

std::optional<LanguageResource> parse_language_resource(
    const std::span<const std::byte> bytes)
{
    if (bytes.size() < kObjectBase + kEntryTableOffset ||
        !std::equal(kLanguageMagic.begin(), kLanguageMagic.end(), bytes.begin())) {
        return std::nullopt;
    }

    const auto language_name = read_c_string(bytes, kObjectBase);
    const auto count = read_u32_le(bytes, kObjectBase + kEntryCountOffset);
    if (!language_name.has_value() || !count.has_value()) {
        return std::nullopt;
    }
    const auto table_offset = kObjectBase + kEntryTableOffset;
    const auto table_bytes = static_cast<std::size_t>(*count);
    if (table_bytes > (bytes.size() - table_offset) / kEntrySize) {
        return std::nullopt;
    }
    const auto table_end = table_offset + table_bytes * kEntrySize;

    LanguageResource resource{};
    resource.language_name = *language_name;
    resource.entries.reserve(*count);
    for (std::size_t index = 0; index < table_bytes; ++index) {
        const auto pair_offset = table_offset + index * kEntrySize;
        const auto key_relative = read_u32_le(bytes, pair_offset);
        const auto value_relative = read_u32_le(bytes, pair_offset + 4U);
        if (!key_relative.has_value() || !value_relative.has_value()) {
            return std::nullopt;
        }
        const auto key_offset_wide = static_cast<std::uint64_t>(kObjectBase)
            + *key_relative;
        const auto value_offset_wide = static_cast<std::uint64_t>(kObjectBase)
            + *value_relative;
        if (key_offset_wide < table_end || value_offset_wide < table_end ||
            key_offset_wide >= bytes.size() || value_offset_wide >= bytes.size()) {
            return std::nullopt;
        }
        const auto key_offset = static_cast<std::size_t>(key_offset_wide);
        const auto value_offset = static_cast<std::size_t>(value_offset_wide);
        const auto key = read_c_string(bytes, key_offset);
        const auto value = read_c_string(bytes, value_offset);
        if (!key.has_value() || !value.has_value()) {
            return std::nullopt;
        }
        resource.entries.push_back({*key, *value});
    }
    return resource;
}

LanguageResource load_language_resource(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("unable to open language resource: " + path.string());
    }
    const auto end = input.tellg();
    if (end < std::streampos{0}) {
        throw std::runtime_error("language resource size is not representable: " + path.string());
    }
    const auto size = static_cast<std::streamoff>(end);
    if (static_cast<std::uintmax_t>(size) >
            static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max()) ||
        size > std::numeric_limits<std::streamsize>::max()) {
        throw std::runtime_error("language resource size is not representable: " + path.string());
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    input.seekg(0, std::ios::beg);
    if (!bytes.empty() && !input.read(reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()))) {
        throw std::runtime_error("unable to read language resource: " + path.string());
    }
    auto parsed = parse_language_resource(bytes);
    if (!parsed.has_value()) {
        throw std::invalid_argument("language resource has an invalid LANGRES0 layout: " +
                                    path.string());
    }
    return std::move(*parsed);
}

} // namespace viper
