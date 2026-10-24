#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace viper {

struct LanguageResourceEntry {
    std::string key;
    std::string value;
};

struct LanguageResource {
    std::string language_name;
    std::vector<LanguageResourceEntry> entries;

    [[nodiscard]] std::optional<std::string_view> find(
        std::string_view key) const noexcept;
};

// Parses the LANGRES0 key/value offset table without applying native pointer
// rebasing or writing to the input bytes.
[[nodiscard]] std::optional<LanguageResource> parse_language_resource(
    std::span<const std::byte> bytes);

[[nodiscard]] LanguageResource load_language_resource(
    const std::filesystem::path& path);

} // namespace viper
