#include "viper/replay_save.hpp"

#include <array>

namespace viper {
namespace {

struct LabelSpec {
    std::uint8_t bit;
    std::uint32_t destination;
    std::uint32_t string;
    std::uint32_t cleanup;
};

constexpr std::array<LabelSpec, 4> kSaveLabels{{
    {0x01U, 0x004ec7d8U, 0x004d0968U, 0x00407eb0U},
    {0x02U, 0x004ec540U, 0x004d0980U, 0x00407ea0U},
    {0x04U, 0x004eb818U, 0x004d099cU, 0x00407e90U},
    {0x08U, 0x004eb7e0U, 0x004d09bcU, 0x00407e80U},
}};

constexpr std::array<std::uint32_t, 2> kPreDialogCacheObjects{
    0x004ec540U, 0x004ec7d8U};
constexpr std::array<std::uint32_t, 2> kOpenFailureCacheObjects{
    0x004eb7e0U, 0x004eb818U};

bool append_cache_refreshes(
    ReplaySavePlan& plan,
    const std::array<std::optional<bool>, 2>& comparisons,
    const std::array<std::uint32_t, 2>& objects)
{
    for (std::size_t i = 0; i < comparisons.size(); ++i) {
        if (!comparisons[i].has_value()) {
            plan.input_complete = false;
            return false;
        }
        plan.cache_refreshes.push_back({objects[i], *comparisons[i]});
        if (*comparisons[i]) {
            plan.calls.push_back(0x418cc0U);
        }
    }
    return true;
}

} // namespace

ReplaySavePlan plan_replay_save(const ReplaySaveObservation& observation)
{
    ReplaySavePlan result{};
    if (!observation.replay_visible_0x4d0668) {
        result.exit = ReplaySaveExit::replay_not_visible;
        return result;
    }

    // 0x4265C0 tears down the current replay before the save flow continues.
    result.calls.insert(result.calls.end(),
                        {0x4265c0U, 0x415090U, 0x4257d0U,
                         0x46b8f0U, 0x4150a0U});
    result.replay_torn_down = true;

    // 0x407E70 wraps 0x4119B0(&DAT_004EC820); its low-byte result is ignored.
    result.calls.insert(result.calls.end(), {0x407e70U, 0x4119b0U});

    if (!observation.label_mask_0x4ec52c.has_value()) {
        result.input_complete = false;
        return result;
    }
    result.label_mask_after = *observation.label_mask_0x4ec52c;
    for (const auto& label : kSaveLabels) {
        if ((result.label_mask_after & label.bit) != 0U) {
            continue;
        }
        result.label_mask_after = static_cast<std::uint8_t>(
            result.label_mask_after | label.bit);
        result.calls.push_back(0x418c90U);
        result.calls.push_back(0x4bd130U);
        result.label_registrations.push_back(
            {label.bit, label.destination, label.string, label.cleanup});
    }

    if (!append_cache_refreshes(result,
                                observation.pre_dialog_cache_refresh_needed,
                                kPreDialogCacheObjects)) {
        return result;
    }

    result.calls.push_back(0x474770U);
    result.save_dialog_called = true;
    if (!observation.dialog_accepted.has_value()) {
        result.input_complete = false;
        return result;
    }
    if (!*observation.dialog_accepted) {
        result.exit = ReplaySaveExit::dialog_cancelled;
        return result;
    }

    result.calls.push_back(0x4112a0U);
    result.file_open_attempted = true;
    if (!observation.file_opened.has_value()) {
        result.input_complete = false;
        return result;
    }
    if (!*observation.file_opened) {
        if (!append_cache_refreshes(
                result, observation.open_failure_cache_refresh_needed,
                kOpenFailureCacheObjects)) {
            return result;
        }
        result.calls.push_back(0x472f00U);
        result.open_failure_dialog_shown = true;
        result.exit = ReplaySaveExit::file_open_failed;
        return result;
    }

    // The callback ignores WriteFile's result. It writes the current fixed
    // replay record, then delegates the variable replay payload to 0x426960.
    result.calls.push_back(0x4116e0U);
    result.record_write_call_made = true;
    result.record_bytes = 0xcd4U;
    result.calls.push_back(0x426960U);
    result.replay_payload_serializer_called = true;
    result.payload_magic = 0x5245504cU;
    result.payload_version = 6U;
    result.payload_writes.push_back({0U, 0x1cU});

    std::optional<std::uint32_t> word_payload_bytes;
    if (observation.payload_word_count_0x508a58.has_value()) {
        word_payload_bytes = *observation.payload_word_count_0x508a58 * 4U;
    } else {
        result.input_complete = false;
    }
    result.payload_writes.push_back({0x00508a70U, word_payload_bytes});

    if (observation.payload_tail_bytes_0x508a3c.has_value()) {
        result.payload_writes.push_back(
            {0x00508a7cU, *observation.payload_tail_bytes_0x508a3c});
    } else {
        result.payload_writes.push_back({0x00508a7cU, std::nullopt});
        result.input_complete = false;
    }

    result.calls.push_back(0x411500U);
    result.file_handle_closed = true;
    result.exit = ReplaySaveExit::saved;
    return result;
}

} // namespace viper
