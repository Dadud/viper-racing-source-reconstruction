#include "viper/asset_catalog.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace viper {

AssetCatalog::AssetCatalog(std::filesystem::path root)
    : root_(std::move(root)) {}

AssetCatalog AssetCatalog::open(const std::filesystem::path& root)
{
    if (root.empty()) {
        throw std::invalid_argument("asset directory path is empty");
    }

    const auto absolute_root = std::filesystem::absolute(root);
    const auto canonical_root = std::filesystem::canonical(absolute_root);
    if (!std::filesystem::is_directory(canonical_root)) {
        throw std::invalid_argument("asset path is not a directory: " +
                                    canonical_root.string());
    }
    return AssetCatalog(canonical_root);
}

std::vector<AssetFile> AssetCatalog::enumerate() const
{
    std::vector<AssetFile> files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root_)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        files.push_back({entry.path().lexically_relative(root_), entry.file_size()});
    }

    std::sort(files.begin(), files.end(), [](const AssetFile& left,
                                              const AssetFile& right) {
        return left.relative_path.generic_string() <
               right.relative_path.generic_string();
    });
    return files;
}

} // namespace viper
