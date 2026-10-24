#include "viper/race_progression.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <array>
#include <vector>

namespace {

void require(const bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

} // namespace

int main()
{
    try {
        viper::RaceProgressionState state{};
        viper::initialize_race_progression(state, 1U);
        viper::advance_race_clock(state, 0.5F);
        require(!viper::observe_race_route_transition(state, 2U, true, 2U),
            "ordinary forward segment changes must not complete a lap");
        require(!viper::observe_race_route_transition(state, 0U, true, 2U),
            "crossing the route array boundary away from the lap start must not count");
        require(viper::observe_race_route_transition(state, 1U, true, 2U) &&
                state.completed_laps == 1U && !state.finished,
            "crossing the initialized lap start must count one lap");
        require(!viper::observe_race_route_transition(state, 1U, false, 2U),
            "reverse travel must not count as a completed lap");
        viper::advance_race_clock(state, 1.25F);
        require(viper::observe_race_route_transition(state, 1U, true, 2U),
            "the second forward crossing must reach the configured finish");
        require(state.finished && state.completed_laps == 2U &&
                std::abs(state.elapsed_seconds - 1.75F) < 1.0e-6F,
            "configured lap count must finish the race and retain elapsed time");
        viper::advance_race_clock(state, 4.0F);
        require(state.elapsed_seconds == 1.75F &&
                !viper::observe_race_route_transition(state, 1U, true, 2U),
            "finished race state must remain stable");

        const std::vector<std::array<float, 2>> square_route{
            {0.0F, 0.0F}, {10.0F, 0.0F},
            {10.0F, 10.0F}, {0.0F, 10.0F}};
        viper::RouteMotionState motion{};
        viper::initialize_race_progression(motion.race, 0U);
        viper::RouteMotionResult motion_result{};
        for (unsigned frame = 0U; frame < 6000U && !motion.race.finished; ++frame) {
            motion_result = viper::advance_route_motion(motion, square_route,
                false, false, true, false, 1.0F / 60.0F, 2U);
            require(motion_result.valid,
                "route integrator must produce finite poses during a closed-course run");
        }
        require(motion.race.finished && motion.race.completed_laps == 2U &&
                motion_result.finished && motion.race.elapsed_seconds > 0.0F,
            "the runtime route integrator must drive through the configured finish");
        std::cout << "race progression regression checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
