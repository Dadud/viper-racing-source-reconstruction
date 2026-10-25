#include "viper/graphics_startup.hpp"

#include <array>
#include <iostream>
#include <stdexcept>

namespace {

void require(const bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

} // namespace

int main()
{
    try {
        const auto observed_failure = viper::plan_directdraw_video_memory_class({
            0xFFFF0000U, 0xC0000000U, 0x3FFF0000U});
        require(observed_failure.adjusted_video_memory_total == 0x00086000U,
            "the native DWORD memory adjustment must wrap exactly");
        require(observed_failure.initial_memory_class == 0U &&
                observed_failure.final_memory_class == 0U &&
                !observed_failure.supported_memory_class &&
                observed_failure.process_exit_code == 5U,
            "the observed 0xffff0000 driver total must project to native exit 5");
        require(observed_failure.agp_texture_memory_present &&
                !observed_failure.resolution_flags_cleared[2],
            "an unsupported base class must not reach the supported-class flag clears");

        const auto instrumented_startup = viper::plan_directdraw_video_memory_class({
            0x00200000U, 0xC0000000U, 0x3FFF0000U});
        require(instrumented_startup.adjusted_video_memory_total == 0x00296000U &&
                instrumented_startup.initial_memory_class == 2U &&
                instrumented_startup.final_memory_class == 4U,
            "the captured process-only total override plus AGP report must classify as 4");
        require(instrumented_startup.supported_memory_class &&
                !instrumented_startup.process_exit_code &&
                (instrumented_startup.resolution_flags_cleared ==
                    std::array<bool, 5>{false, false, false, true, true}),
            "class 4 must pass startup and clear only the observed 800/1024 flags");

        const auto class_16 = viper::plan_directdraw_video_memory_class({
            16'000'000U - 0x96000U, 2'000'000U, 0U});
        require(class_16.final_memory_class == 16U &&
                class_16.supported_memory_class &&
                !class_16.resolution_flags_cleared[2] &&
                !class_16.resolution_flags_cleared[3] &&
                !class_16.resolution_flags_cleared[4],
            "the 16 MB class must retain every enumerated resolution flag");
        std::cout << "DirectDraw video-memory classification regression passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
