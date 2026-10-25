#include "viper/setup_preferences.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures{};

void check(const bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

} // namespace

int main()
{
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = std::filesystem::temp_directory_path() /
        ("viper-setup-preferences-" + std::to_string(nonce));
    const auto missing = viper::load_setup_selection_preference(root);
    check(missing.file_missing && !missing.selection && missing.error.empty(),
        "missing preference is a safe first-run state");

    const auto first_save = viper::save_setup_selection_preference(root,
        {"Bemidji", "Sports"});
    check(first_save.saved && first_save.path == root / "viper-racing-setup.cfg",
        "writes normalized selector identifiers under Config");
    const auto loaded = viper::load_setup_selection_preference(root);
    check(loaded.selection && loaded.selection->track == "bemidji" &&
        loaded.selection->car == "sports", "round-trips selected track and car");

    const std::vector<std::string> tracks{"bemidji", "dundas"};
    const std::vector<std::string> cars{"viper", "sports"};
    const auto resolved = viper::resolve_setup_selection_indices(
        tracks, cars, loaded.selection, "dundas", "viper");
    check(resolved.track == 0U && resolved.car == 1U,
        "saved identifiers override current options.def defaults");

    const auto replaced = viper::save_setup_selection_preference(root,
        {"removed_track", "Viper"});
    check(replaced.saved, "atomically replaces existing selector values");
    const auto stale = viper::load_setup_selection_preference(root);
    const auto stale_resolved = viper::resolve_setup_selection_indices(
        tracks, cars, stale.selection, "dundas", "sports");
    check(stale_resolved.track == 1U && stale_resolved.car == 0U,
        "stale track falls back while a valid saved car remains selected");

    {
        std::ofstream corrupt(root / "viper-racing-setup.cfg", std::ios::binary);
        corrupt << "VIPER-RACING-SETUP 1\ntrack=../outside\ncar=sports\n";
    }
    const auto invalid = viper::load_setup_selection_preference(root);
    check(!invalid.selection && !invalid.error.empty(),
        "rejects invalid path-like stored stems");
    const auto invalid_save = viper::save_setup_selection_preference(root,
        {"../outside", "sports"});
    check(!invalid_save.saved && !invalid_save.error.empty(),
        "refuses to write path-like resource stems");

    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    return failures == 0 ? 0 : 1;
}
