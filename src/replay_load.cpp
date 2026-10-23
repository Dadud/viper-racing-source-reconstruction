#include "viper/replay_load.hpp"

#include <utility>

namespace viper {

ReplayLoadPlan plan_replay_load(const ReplayLoadObservation& observation)
{
    ReplayLoadPlan result{};
    if (observation.replay_ui_guard_0x4ec78c) {
        result.exit = ReplayLoadExit::modal_guard;
        result.calls.push_back(0x411000U);
        return result;
    }

    if (observation.current_replay_visible_0x4d0668) {
        result.calls.push_back(0x4265c0U);
        result.calls.push_back(0x46b8f0U);
    }

    if (!observation.current_replay_path_present_0x4ec5f0) {
        result.calls.push_back(0x474220U);
        if (!observation.dialog_accepted.has_value()) {
            result.input_complete = false;
            return result;
        }
        if (!*observation.dialog_accepted) {
            result.exit = ReplayLoadExit::dialog_cancelled;
            result.calls.push_back(0x410ea0U);
            return result;
        }
    }

    result.calls.push_back(0x411430U);
    if (!observation.file_opened.has_value()) {
        result.input_complete = false;
        return result;
    }
    if (!*observation.file_opened) {
        result.exit = ReplayLoadExit::file_open_failed;
        result.calls.push_back(0x472f00U);
        return result;
    }

    result.calls.push_back(0x40c950U);
    if (observation.current_replay_visible_0x4d0668) {
        result.calls.insert(result.calls.end(),
                            {0x426560U, 0x45ba40U, 0x46b500U,
                             0x4178c0U, 0x40a760U});
        result.current_replay_cleared = true;
    }

    result.calls.push_back(0x411560U);
    if (!observation.record_read.has_value()) {
        result.input_complete = false;
        return result;
    }
    if (!*observation.record_read) {
        result.exit = ReplayLoadExit::data_read_failed;
    } else {
        result.record_buffer_read = true;
        result.record_buffer_bytes = 0xcd4U;
        result.calls.push_back(0x45c590U);
        if (!observation.record_valid.has_value()) {
            result.input_complete = false;
            return result;
        }
        if (!*observation.record_valid) {
            result.exit = ReplayLoadExit::invalid_record;
            result.calls.push_back(0x472f00U);
        } else {
            result.calls.insert(result.calls.end(),
                                {0x46b4c0U, 0x417740U, 0x40a710U,
                                 0x45b8d0U, 0x426520U});
            result.replay_record_installed = true;
            result.calls.push_back(0x426790U);
            std::optional<bool> replay_started = observation.replay_started;
            if (observation.file_parser.has_value()) {
                auto parse = plan_replay_file_load(*observation.file_parser);
                result.calls.insert(result.calls.end(), parse.calls.begin(),
                                    parse.calls.end());
                result.input_complete = result.input_complete &&
                    parse.input_complete;
                replay_started = parse.exit == ReplayFileLoadExit::loaded;
                result.file_parse = std::move(parse);
                if (!result.file_parse->input_complete) {
                    return result;
                }
            }
            if (!replay_started.has_value()) {
                result.input_complete = false;
                return result;
            }
            if (*replay_started) {
                result.exit = ReplayLoadExit::replay_started;
            } else {
                result.exit = ReplayLoadExit::replay_start_failed;
                result.calls.insert(result.calls.end(),
                                    {0x410ea0U, 0x426560U, 0x40a760U,
                                     0x45ba40U, 0x46b500U, 0x472f00U});
                result.replay_record_installed = false;
                result.replay_record_removed_after_start_failure = true;
            }
        }
    }

    result.calls.push_back(0x411500U);
    result.file_handle_closed = true;
    result.calls.push_back(0x450c00U);
    result.calls.push_back(0x4487a0U);
    result.renderer_restored = true;
    return result;
}

} // namespace viper
