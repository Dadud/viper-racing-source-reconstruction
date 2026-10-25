#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace viper {

class GameOptions {
public:
    [[nodiscard]] static GameOptions parse(std::string_view contents);
    [[nodiscard]] static GameOptions load(const std::filesystem::path& path);

    [[nodiscard]] std::optional<std::string_view> get(
        std::string_view section, std::string_view key) const noexcept;
    [[nodiscard]] std::optional<int> integer(
        std::string_view section, std::string_view key) const noexcept;
    [[nodiscard]] std::optional<float> real(
        std::string_view section, std::string_view key) const noexcept;
    [[nodiscard]] std::optional<bool> boolean(
        std::string_view section, std::string_view key) const noexcept;
    void set_string(std::string section, std::string key, std::string value);
    void set_integer(std::string section, std::string key, int value);
    [[nodiscard]] std::string serialize() const;
    void save(const std::filesystem::path& path) const;
    [[nodiscard]] const std::map<std::string, std::map<std::string, std::string>>& entries()
        const noexcept { return entries_; }

private:
    std::map<std::string, std::map<std::string, std::string>> entries_;
};

} // namespace viper
