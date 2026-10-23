#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace viper {

struct AssetFile {
    std::filesystem::path relative_path;
    std::uintmax_t size_bytes{};
};

class AssetCatalog {
public:
    static AssetCatalog open(const std::filesystem::path& root);

    const std::filesystem::path& root() const noexcept { return root_; }
    std::vector<AssetFile> enumerate() const;

private:
    explicit AssetCatalog(std::filesystem::path root);

    std::filesystem::path root_;
};

} // namespace viper
