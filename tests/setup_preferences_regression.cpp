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
        {"Bemidji", "Sports", 3U});
    check(first_save.saved && first_save.path == root / "viper-racing-setup.cfg",
        "writes normalized selector identifiers under Config");
    const auto loaded = viper::load_setup_selection_preference(root);
    check(loaded.selection && loaded.selection->track == "bemidji" &&
        loaded.selection->car == "sports" && loaded.selection->laps == 3U &&
        loaded.selection->opponent_strength == 0U,
        "round-trips selected track, car, lap count, and opponent profile");

    const std::vector<std::string> tracks{"bemidji", "dundas"};
    const std::vector<std::string> cars{"viper", "sports"};
    const auto resolved = viper::resolve_setup_selection_indices(
        tracks, cars, loaded.selection, "dundas", "viper");
    check(resolved.track == 0U && resolved.car == 1U,
        "saved identifiers override current options.def defaults");

    {
        std::ofstream legacy(root / "viper-racing-setup.cfg", std::ios::binary);
        legacy << "VIPER-RACING-SETUP 1\ntrack=dundas\ncar=viper\n";
    }
    const auto legacy = viper::load_setup_selection_preference(root);
    check(legacy.selection && legacy.selection->track == "dundas" &&
        legacy.selection->car == "viper" && !legacy.selection->laps &&
        !legacy.selection->opponent_strength,
        "loads version-1 preferences and leaves lap count at the game default");

    {
        std::ofstream legacy_v2(root / "viper-racing-setup.cfg", std::ios::binary);
        legacy_v2 << "VIPER-RACING-SETUP 2\ntrack=dundas\ncar=viper\nlaps=4\n";
    }
    const auto legacy_v2 = viper::load_setup_selection_preference(root);
    check(legacy_v2.selection && legacy_v2.selection->laps == 4U &&
        !legacy_v2.selection->opponent_strength,
        "loads version-2 lap preferences with the options.def profile default");

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
    const auto invalid_laps_save = viper::save_setup_selection_preference(root,
        {"bemidji", "sports", 100U});
    check(!invalid_laps_save.saved && !invalid_laps_save.error.empty(),
        "refuses to write lap counts above the supported 1-99 range");
    {
        std::ofstream invalid_laps(root / "viper-racing-setup.cfg", std::ios::binary);
        invalid_laps << "VIPER-RACING-SETUP 2\ntrack=bemidji\ncar=sports\nlaps=100\n";
    }
    const auto invalid_laps = viper::load_setup_selection_preference(root);
    check(!invalid_laps.selection && !invalid_laps.error.empty(),
        "rejects an out-of-range lap count read from Config");
    {
        std::ofstream invalid_profile(root / "viper-racing-setup.cfg", std::ios::binary);
        invalid_profile << "VIPER-RACING-SETUP 3\ntrack=bemidji\ncar=sports\n"
            "laps=3\nopponent_strength=8\n";
    }
    const auto invalid_profile = viper::load_setup_selection_preference(root);
    check(!invalid_profile.selection && !invalid_profile.error.empty(),
        "rejects an out-of-range opponent profile read from Config");
    const auto invalid_profile_save = viper::save_setup_selection_preference(root,
        {"bemidji", "sports", 3U, 8U});
    check(!invalid_profile_save.saved && !invalid_profile_save.error.empty(),
        "refuses to write opponent profiles outside the eight native groups");

    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    return failures == 0 ? 0 : 1;
}
