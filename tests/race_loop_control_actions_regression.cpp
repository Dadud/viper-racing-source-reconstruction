#include "viper/race_loop_callbacks.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

int main()
{
    viper::RaceLoopControlState state{};
    state.counter_0x5093bc = 1;
    viper::RaceLoopControlObservation observation{};
    observation.frame_delta = 0.1F;
    observation.value_0x5093b0 = 0.25F;
    observation.value_0x509400 = 0.1F;
    observation.value_0x509378 = 0.7F;
    observation.value_0x5093d8 = 0.8F;
    observation.value_0x509418 = 0.2F;
    observation.value_0x509430 = 1.0F;
    observation.counter_limit_0x50935c = 3;
    observation.input_0x509408_active = true;
    observation.input_0x509430_uses_ramped_path = true;
    observation.input_0x509430_active = true;
    observation.input_0x5093d0_active = true;
    observation.input_0x509398_active = true;
    observation.input_0x5093e8_active = true;
    observation.input_0x5093e0_active = true;
    observation.input_0x509418_active = true;
    observation.input_0x5093d8_active = false;

    viper::update_race_loop_control_state(state, observation);
    if (state.value_0x5093b8 != 0.25F || state.flag_0x5093f0 != 1U ||
        std::fabs(state.value_0x5093c8 - 1.0F) > 1.0e-6F ||
        state.flag_0x5093cc != 1U || state.flag_0x5093f8 != 1U ||
        state.flag_0x509360 != 1U ||
        std::fabs(state.difference_0x509344 - 0.6F) > 1.0e-6F ||
        state.flag_0x509420 != 1U ||
        std::fabs(state.difference_0x509340 - 0.6F) > 1.0e-6F ||
        state.transition_latch_0x509354 != 1U ||
        state.counter_0x5093bc != 2) {
        std::cerr << "Observed race control outputs or first shift edge were incorrect.\n";
        return EXIT_FAILURE;
    }

    // A held shift key is edge-latched. Releasing both inputs rearms it.
    viper::update_race_loop_control_state(state, observation);
    if (state.counter_0x5093bc != 2) {
        std::cerr << "Held shift input repeated without a release edge.\n";
        return EXIT_FAILURE;
    }
    observation.input_0x509418_active = false;
    observation.input_0x5093d8_active = false;
    viper::update_race_loop_control_state(state, observation);
    if (state.transition_latch_0x509354 != 0U) {
        std::cerr << "Releasing both shift inputs did not clear the edge latch.\n";
        return EXIT_FAILURE;
    }
    observation.input_0x5093d8_active = true;
    viper::update_race_loop_control_state(state, observation);
    if (state.counter_0x5093bc != 1 || state.transition_latch_0x509354 != 1U) {
        std::cerr << "Downshift edge did not decrement exactly once.\n";
        return EXIT_FAILURE;
    }

    // Reverse ramps up by 10 per second and decays by 5 per second.
    observation.input_0x509430_active = true;
    observation.value_0x509430 = 0.0F;
    viper::update_race_loop_control_state(state, observation);
    if (std::fabs(state.value_0x5093c8 - 1.0F) > 1.0e-6F) {
        std::cerr << "Reverse input did not reach its recovered upper clamp.\n";
        return EXIT_FAILURE;
    }
    observation.input_0x509430_active = false;
    viper::update_race_loop_control_state(state, observation);
    if (std::fabs(state.value_0x5093c8 - 0.5F) > 1.0e-6F) {
        std::cerr << "Reverse input did not decay at the recovered rate.\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
