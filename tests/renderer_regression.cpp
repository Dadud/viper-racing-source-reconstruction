#include "viper/scene_car_update.hpp"

#include <array>
#include <cstdint>
#include <exception>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void check(const bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

using TargetPair = std::array<std::uint32_t, 2>;

void test_selector_matrix()
{
    using namespace viper;
    struct Case {
        bool enabled;
        bool mode119;
        bool raster;
        bool mode118;
        TargetPair expected;
    };
    // Recovered 0x455C80 branch table, ordered as callback slots +0x4DB744/+0x4DB748.
    constexpr std::array cases{
        Case{false, false, false, false, {0x00455D90U, 0x00455D90U}},
        Case{true, false, false, false, {0x00456230U, 0x00455E00U}},
        Case{true, false, false, true,  {0x004563E0U, 0x00455F20U}},
        Case{true, false, true,  false, {0x00456660U, 0x00455F30U}},
        Case{true, false, true,  true,  {0x00456660U, 0x00455F30U}},
        Case{true, true,  false, false, {0x004569C0U, 0x00455F40U}},
        Case{true, true,  false, true,  {0x00456D00U, 0x00456210U}},
        Case{true, true,  true,  false, {0x00457090U, 0x00456220U}},
        Case{true, true,  true,  true,  {0x00457090U, 0x00456220U}},
    };
    for (const auto& test : cases) {
        const auto selected = plan_scene_render_callback_selection(test.enabled,
            {test.mode119, test.raster, test.mode118});
        check(TargetPair{selected.callback_slot_0x4db744, selected.callback_slot_0x4db748}
                == test.expected,
            "selector matrix mismatch for enabled/mode119/raster/mode118 case");
    }

    SceneRendererSubmitTargetSelectionInput unknown{};
    const auto enabled_unknown = plan_scene_renderer_submit_targets(unknown);
    check(!enabled_unknown.complete && !enabled_unknown.target_0x4db744
            && !enabled_unknown.target_0x4db748,
        "unknown enable flag must leave target selection incomplete");
    unknown.enabled_0x50d0a8 = false;
    const auto disabled = plan_scene_renderer_submit_targets(unknown);
    check(disabled.complete && disabled.target_0x4db744 == 0x00455D90U
            && disabled.target_0x4db748 == 0x00455D90U,
        "disabled selector must not require any downstream observations");

    unknown.enabled_0x50d0a8 = true;
    unknown.mode_0x50d119 = true;
    unknown.raster_mode_0x50d2dc = 2;
    unknown.mode_0x50d118.reset();
    const auto nonzero_raster = plan_scene_renderer_submit_targets(unknown);
    check(nonzero_raster.complete && nonzero_raster.target_0x4db744 == 0x00457090U
            && nonzero_raster.target_0x4db748 == 0x00456220U,
        "nonzero raster selector branch must not require mode 0x50D118");

    unknown.raster_mode_0x50d2dc = 0;
    const auto zero_raster = plan_scene_renderer_submit_targets(unknown);
    check(!zero_raster.complete,
        "zero raster selector branch must require mode 0x50D118");
}

std::array<viper::SceneRenderRowRecord, 2> make_source()
{
    return {{
        {0x10U, 0x11U, 0x12U, 0x13U, 0x14U, 0x15U, 0x16U, 0x17U},
        {0x20U, 0x21U, 0x22U, 0x23U, 0x24U, 0x25U, 0x26U, 0x27U},
    }};
}

std::array<viper::SceneRenderRowRecord, 2> make_destination()
{
    return {{
        {0xA0U, 0xA1U, 0xA2U, 0xA3U, 0xA4U, 0xA5U, 0xA6U, 0xA7U},
        {0xB0U, 0xB1U, 0xB2U, 0xB3U, 0xB4U, 0xB5U, 0xB6U, 0xB7U},
    }};
}

viper::SceneRenderScanlineConversionObservation base_observation(
    const std::array<viper::SceneRenderRowRecord, 2>& source,
    const std::array<viper::SceneRenderRowRecord, 2>& destination)
{
    viper::SceneRenderScanlineConversionObservation observation{};
    observation.first_row = 0;
    observation.end_row = 1;
    observation.source_rows = source;
    observation.destination_before = destination;
    return observation;
}

void expect_write(const viper::SceneRenderScanlineConversionPlan& plan,
    const std::size_t index, const std::uint8_t offset, const std::uint32_t value,
    const std::string& context)
{
    check(index < plan.writes.size(), context + ": missing ordered write");
    const auto& write = plan.writes[index];
    check(write.row_index == 0 && write.byte_offset == offset && write.value == value,
        context + ": write order/value mismatch at operation " + std::to_string(index));
}

void test_environment_palette_callback()
{
    using namespace viper;
    const auto source = make_source();
    const auto destination = make_destination();
    constexpr std::array<std::uint32_t, 1> palette_indices{0U};
    constexpr std::array<std::uint32_t, 1> palette_words{0x12340000U};
    constexpr std::array<std::uint8_t, 1> alpha{0x5AU};
    constexpr std::array<std::array<std::uint32_t, 2>, 1> tails{{
        {0xCAFEBABEU, 0xDEADC0DEU}}};

    auto observation = base_observation(source, destination);
    observation.descriptor_mode = 1;
    observation.color_or_mask_0x50d114 = 0x0000000FU;
    observation.lighting_palette_indices = palette_indices;
    observation.lighting_palette_words = palette_words;
    observation.distance_fade_alpha_bytes = alpha;
    observation.environment_mapping_mode_2_enabled = false;
    auto plan = plan_scene_render_scanline_conversion(0x00457090U, observation);
    check(plan.status == SceneRenderScanlineConversionStatus::complete,
        "0x457090 disabled environment conversion should complete");
    constexpr std::array<std::uint8_t, 8> offsets{
        0x00U, 0x04U, 0x08U, 0x18U, 0x1CU, 0x10U, 0x14U, 0x14U};
    constexpr std::array<std::uint32_t, 8> values{
        0x10U, 0x11U, 0x12U, 0x16U, 0x17U, 0x1234000FU, 0U, 0x5A000000U};
    check(plan.writes.size() == offsets.size(), "0x457090 must emit eight writes for mode 1");
    for (std::size_t i = 0; i < offsets.size(); ++i) {
        expect_write(plan, i, offsets[i], values[i], "0x457090 normal branch");
    }
    check(plan.destination_after[0][3] == destination[0][3],
        "0x457090 must preserve destination +0x0C");
    check(plan.destination_after[0][6] == source[0][6]
            && plan.destination_after[0][7] == source[0][7],
        "environment-disabled branch must retain copied tail words");
    check(plan.rows_processed == 1 && plan.row_counter_increments == 1
            && plan.environment_mapping_transform_rows == 0,
        "environment-disabled branch row accounting mismatch");

    observation.environment_mapping_mode_2_enabled = true;
    observation.transformed_tail_words = tails;
    plan = plan_scene_render_scanline_conversion(0x00457090U, observation);
    check(plan.status == SceneRenderScanlineConversionStatus::complete
            && plan.environment_mapping_transform_rows == 1,
        "0x457090 enabled environment conversion should write transformed tail");
    check(plan.destination_after[0][6] == tails[0][0]
            && plan.destination_after[0][7] == tails[0][1],
        "environment-enabled branch must overwrite copied tail words");
    check(plan.writes.size() == 10, "environment-enabled branch must append two tail writes");
    expect_write(plan, 8, 0x18U, tails[0][0], "0x457090 transformed tail");
    expect_write(plan, 9, 0x1CU, tails[0][1], "0x457090 transformed tail");

    observation.environment_mapping_mode_2_enabled.reset();
    plan = plan_scene_render_scanline_conversion(0x00457090U, observation);
    check(plan.status == SceneRenderScanlineConversionStatus::environment_mapping_mode_observation_exhausted
            && plan.rows_processed == 0 && plan.row_counter_increments == 0,
        "missing environment mode must stop before row accounting");
    check(plan.writes.size() == 8 && plan.destination_after[0][5] == 0x5A000000U,
        "missing environment mode must preserve all writes before its branch read");

    observation.environment_mapping_mode_2_enabled = true;
    observation.transformed_tail_words = {};
    plan = plan_scene_render_scanline_conversion(0x00457090U, observation);
    check(plan.status == SceneRenderScanlineConversionStatus::transformed_tail_observation_exhausted
            && plan.rows_processed == 0 && plan.row_counter_increments == 0,
        "missing transformed tail must stop before row accounting");
    check(plan.writes.size() == 8 && plan.destination_after[0][5] == 0x5A000000U,
        "missing transformed tail must preserve writes made before its bounds check");

    observation.transformed_tail_words = tails;
    observation.distance_fade_alpha_bytes = {};
    plan = plan_scene_render_scanline_conversion(0x00457090U, observation);
    check(plan.status == SceneRenderScanlineConversionStatus::distance_fade_alpha_observation_exhausted
            && plan.rows_processed == 0 && plan.row_counter_increments == 0,
        "missing alpha must leave row counter unchanged");
    check(plan.writes.size() == 7 && plan.destination_after[0][5] == 0U,
        "missing alpha must retain zero-initialized alpha word after preceding copies");
    check(plan.destination_after[0][3] == destination[0][3],
        "missing alpha branch must preserve destination +0x0C");
}

void test_mode_two_and_existing_callback()
{
    using namespace viper;
    const auto source = make_source();
    const auto destination = make_destination();
    auto observation = base_observation(source, destination);
    observation.descriptor_mode = 2;
    // Deliberately no palette indices or words: mode 2 selects constant white.
    constexpr std::array<std::uint8_t, 1> alpha{0x7FU};
    observation.distance_fade_alpha_bytes = alpha;
    observation.environment_mapping_mode_2_enabled = false;
    const auto env = plan_scene_render_scanline_conversion(0x00457090U, observation);
    check(env.status == SceneRenderScanlineConversionStatus::complete
            && env.destination_after[0][4] == 0xFFFFFFFFU
            && env.destination_after[0][5] == 0x7F000000U,
        "0x457090 mode 2 must not require palette observations");

    observation = base_observation(source, destination);
    observation.alpha_byte_0x50d110 = 0xA5U;
    const auto alpha_fill = plan_scene_render_scanline_conversion(0x00455D90U, observation);
    check(alpha_fill.status == SceneRenderScanlineConversionStatus::complete
            && alpha_fill.destination_after[0][4] == 0xA5FFFFFFU,
        "existing 0x455D90 alpha-fill callback regression");
    check(alpha_fill.destination_after[0][3] == destination[0][3]
            && alpha_fill.destination_after[0][6] == source[0][6]
            && alpha_fill.destination_after[0][7] == source[0][7],
        "alpha-fill callback must preserve +0x0C and copy tail words");
}

} // namespace

int main()
{
    try {
        test_selector_matrix();
        test_environment_palette_callback();
        test_mode_two_and_existing_callback();
        std::cout << "renderer regression checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "renderer regression failure: " << error.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "renderer regression failure: unknown exception\n";
        return 1;
    }
}
