#pragma once

#include "viper/language_resource.hpp"
#include "viper/resource_bundle.hpp"

#include <array>
#include <filesystem>
#include <string_view>

namespace viper {

// Owns the startup assets used by the runtime for its full session lifetime.
class GameAssets {
public:
    [[nodiscard]] static GameAssets load(const std::filesystem::path& root);

    [[nodiscard]] const std::filesystem::path& root() const noexcept { return root_; }
    [[nodiscard]] const ResourceBundle& bundle(std::string_view name) const;
    [[nodiscard]] const LanguageResource& language() const noexcept { return language_; }

private:
    std::filesystem::path root_;
    std::array<ResourceBundle, 5> bundles_;
    LanguageResource language_;
};

} // namespace viper
