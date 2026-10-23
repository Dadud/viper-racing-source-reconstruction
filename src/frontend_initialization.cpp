#include "viper/frontend_initialization.hpp"

namespace viper {

FrontendInitializationPlan plan_frontend_initialization(
    const FrontendInitializationObservation& observation)
{
    FrontendInitializationPlan plan{};
    const auto call = [&plan](const std::uint32_t address) {
        if (plan.call_count < plan.call_addresses.size()) {
            plan.call_addresses[plan.call_count++] = address;
        }
    };

    call(0x447980U);
    call(0x4482b0U);
    call(0x4479f0U);
    plan.registry_lookup_reached = true;
    call(0x4015e0U);
    if (observation.cd_directory_lookup_succeeded) {
        plan.intro_video_path = observation.cd_directory + "intro.avi";
        plan.intro_path_exceeds_260_byte_local_buffer =
            plan.intro_video_path.size() + 1 > 260;
        plan.intro_video_helper_reached = true;
        call(0x4ba9e0U);
    }

    plan.matrix_global_0x5081a8 = {
        1.0F, 0.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F, 0.0F,
    };
    plan.identity_matrix_written = true;

    call(0x46aa00U);
    call(0x418500U);
    call(0x46b7d0U);
    call(0x417740U);
    call(0x417740U);
    call(0x417740U);
    call(0x406420U);
    call(0x40d110U);
    call(0x4714e0U);
    call(0x49b0f0U);
    call(0x4b6310U);
    call(0x41c880U);
    call(0x412e10U);
    return plan;
}

} // namespace viper
