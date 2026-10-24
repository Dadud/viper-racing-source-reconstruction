#include "viper/scene_overlay.hpp"
#include "viper/scene_font_8x8.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdarg>
#include <cstddef>
#include <cmath>
#include <cstdio>
#include <limits>
#include <utility>

namespace viper {
namespace {

std::int32_t truncate_to_low_i32(const long double value) noexcept
{
    constexpr auto lower = static_cast<long double>(
        std::numeric_limits<std::int64_t>::min());
    constexpr auto upper_exclusive = -lower;
    if (!std::isfinite(value) || value < lower || value >= upper_exclusive) {
        // The x87 FISTP qword invalid-result sentinel has a zero low DWORD.
        return 0;
    }
    const auto truncated = static_cast<std::int64_t>(value);
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(truncated));
}

std::string replace_decimal_marker(std::string value, const char marker)
{
    if (marker != '.') {
        for (auto& character : value) {
            if (character == '.') {
                character = marker;
            }
        }
    }
    return value;
}

std::string format_printf_string(const char* const format, ...)
{
    std::va_list arguments;
    va_start(arguments, format);
    std::va_list counting_arguments;
    va_copy(counting_arguments, arguments);
    const auto required = std::vsnprintf(nullptr, 0, format, counting_arguments);
    va_end(counting_arguments);
    if (required < 0) {
        va_end(arguments);
        return {};
    }

    std::vector<char> buffer(static_cast<std::size_t>(required) + 1);
    std::vsnprintf(buffer.data(), buffer.size(), format, arguments);
    va_end(arguments);
    return std::string(buffer.data(), static_cast<std::size_t>(required));
}

std::optional<SceneDashboardRectangleFillProjection> project_rectangle_fill(
    const std::int32_t left,
    const std::int32_t top,
    const std::int32_t right,
    const std::int32_t bottom,
    const std::uint32_t source_color,
    const RenderContextViewport& context,
    const std::uint8_t renderer_mode,
    const std::uint32_t framebuffer_base)
{
    if (right <= left || bottom <= top ||
        context.viewport_right_0x1c <= left ||
        context.viewport_bottom_0x20 <= top) {
        return std::nullopt;
    }

    const auto clipped_left = left < context.viewport_left_0x14
        ? context.viewport_left_0x14 : left;
    const auto clipped_top = top < context.viewport_top_0x18
        ? context.viewport_top_0x18 : top;
    const auto clipped_right = context.viewport_right_0x1c < right
        ? context.viewport_right_0x1c : right;
    const auto clipped_bottom = context.viewport_bottom_0x20 < bottom
        ? context.viewport_bottom_0x20 : bottom;
    if (clipped_right <= clipped_left || clipped_bottom <= clipped_top) {
        return std::nullopt;
    }

    std::uint32_t encoded_value{};
    std::uint32_t write_mask{};
    std::uint8_t bytes_per_pixel{};
    bool preserves_existing_bits{};
    if (renderer_mode == 1U) {
        encoded_value = (((source_color >> 7U) & 0xF0U) << 4U)
            | ((source_color >> 2U) & 0xF0U)
            | ((source_color & 0x1EU) >> 1U);
        write_mask = 0x0FFFU;
        bytes_per_pixel = 2U;
        preserves_existing_bits = true;
    } else {
        const auto encoding = encode_render_pixel(renderer_mode, source_color);
        encoded_value = encoding.bits;
        bytes_per_pixel = encoding.bytes_per_pixel;
        write_mask = bytes_per_pixel == 4U ? 0xFFFFFFFFU : 0xFFFFU;
    }

    const auto offset = render_context_pixel_offset(
        context, renderer_mode, clipped_left, clipped_top);
    return SceneDashboardRectangleFillProjection{
        clipped_left,
        clipped_top,
        clipped_right,
        clipped_bottom,
        source_color,
        encoded_value,
        write_mask,
        offset,
        framebuffer_base + offset,
        bytes_per_pixel,
        preserves_existing_bits,
    };
}

} // namespace

std::vector<SceneOverlayTextCall> scene_overlay_text_plan(
    const std::int32_t page,
    const std::int32_t selected_car_ordinal)
{
    constexpr std::uint32_t selected_color = 0x004F0B28;
    constexpr std::uint32_t ordinary_color = 0x004F0B08;
    std::vector<SceneOverlayTextCall> calls;

    switch (page) {
    case 0:
        calls.push_back({0, 240, 0x004D3F04, selected_color, -1});
        break;
    case 1:
        calls.push_back({0, 0, 0x004D3EFC, selected_color, -1});
        for (std::int32_t row = 0; row < 8; ++row) {
            const auto color = row == selected_car_ordinal
                ? selected_color
                : ordinary_color;
            calls.push_back({1, 20 + row * 10,
                             0x00508B38 + static_cast<std::uint32_t>(row) * 0x50,
                             color, row});
        }
        break;
    case 2:
        calls.push_back({240, 80, 0x004D3EE4, selected_color, -1});
        break;
    case 3:
        calls.push_back({100, 100, 0x004D3EA0, selected_color, -1});
        calls.push_back({100, 110, 0x004D3EC4, selected_color, -1});
        break;
    default:
        break;
    }
    return calls;
}

bool handle_scene_physics_overlay_key(
    ScenePhysicsOverlayFlags& flags,
    const std::uint16_t key) noexcept
{
    if (key == 0x63) {
        flags.collisions_0x4d4b48 =
            flags.collisions_0x4d4b48 < 1 ? 1 : 0;
        return true;
    }
    if (key == 0x77) {
        flags.walls_0x4d4b4c = flags.walls_0x4d4b4c < 1 ? 1 : 0;
        return true;
    }
    return false;
}

std::vector<SceneOverlayTextCall> scene_physics_overlay_text_plan(
    const ScenePhysicsOverlayFlags& flags)
{
    constexpr std::uint32_t active_color = 0x00508684;
    constexpr std::uint32_t inactive_color = 0x00508678;
    std::vector<SceneOverlayTextCall> calls;
    calls.reserve(8);
    calls.push_back({10, 10, 0x004D4B88, 0x00508650, -1});
    calls.push_back({160, 10, 0x004D4B90,
                     flags.collisions_0x4d4b48 != 0 ? active_color : inactive_color, -1});
    calls.push_back({160, 20, 0x004D4B98,
                     flags.walls_0x4d4b4c != 0 ? active_color : inactive_color, -1});
    calls.push_back({320, 100, 0x004D4BA0, active_color, -1});
    calls.push_back({320, 120, 0x004D4BB4, 0x00508674, -1});
    calls.push_back({320, 140, 0x004D4BD8, 0x00508674, -1});
    calls.push_back({320, 150, 0x004D4C0C, 0x00508674, -1});
    calls.push_back({320, 160, 0x004D4C40, 0x00508674, -1});
    return calls;
}

std::array<ScenePhysicsWheelBarPlan, 4> scene_physics_wheel_bar_plans(
    const std::array<ScenePhysicsWheelBarInput, 4>& inputs)
{
    constexpr long double fill_scale = 40.0L;
    constexpr long double normalization_limit = 1.2L;
    constexpr long double indicator_scale = 20.0L;
    constexpr long double format_offset = 272.0L;
    constexpr long double format_scale = 1.8L;
    constexpr long double format_bias = 32.0L;

    std::array<ScenePhysicsWheelBarPlan, 4> plans{};
    for (std::size_t index = 0; index < inputs.size(); ++index) {
        const auto& input = inputs[index];
        auto& plan = plans[index];
        plan.x = 215 + static_cast<std::int32_t>(index / 2U) * 25;
        plan.y = 295 + static_cast<std::int32_t>(index % 2U) * 50;
        plan.frame_color = input.frame_color_0x508650;
        plan.fill_color = input.fill_color_0x508654;
        plan.indicator_color = input.indicator_color_0x508660;
        plan.fill_width = truncate_to_low_i32(
            static_cast<long double>(input.fill_fraction_0xa4) * fill_scale);
        plan.fill_rectangle_emitted = plan.fill_width > 0;
        if (plan.fill_rectangle_emitted) {
            plan.fill_left = plan.x + 20 - plan.fill_width;
            plan.fill_top = plan.y - 10;
            plan.fill_right = plan.x + 20;
            plan.fill_bottom = plan.y + 10;
        }

        auto component_x = static_cast<long double>(input.vector_component_0x9c);
        auto component_y = static_cast<long double>(input.vector_component_0xa0);
        const auto reference_x =
            static_cast<long double>(input.normalization_reference_x);
        const auto reference_y =
            static_cast<long double>(input.normalization_reference_y);
        const auto reference_magnitude = std::sqrt(
            reference_x * reference_x + reference_y * reference_y);
        if (reference_magnitude > normalization_limit) {
            component_x = component_x / reference_magnitude *
                normalization_limit;
            component_y = component_y / reference_magnitude *
                normalization_limit;
            plan.vector_normalized = true;
        }
        plan.normalized_component_0x9c = static_cast<float>(component_x);
        plan.normalized_component_0xa0 = static_cast<float>(component_y);
        plan.indicator_end_x = plan.x + truncate_to_low_i32(
            component_y * -indicator_scale);
        plan.indicator_end_y = plan.y + truncate_to_low_i32(
            component_x * indicator_scale);
        plan.format_argument = static_cast<double>(
            (static_cast<long double>(input.format_input) - format_offset) *
                format_scale +
            format_bias);
        std::array<char, 64> formatted_label{};
        const auto formatted_label_length = std::snprintf(
            formatted_label.data(), formatted_label.size(), "%1.0f",
            plan.format_argument);
        plan.formatted_label_complete = formatted_label_length >= 0 &&
            static_cast<std::size_t>(formatted_label_length) <
                formatted_label.size();
        plan.fdiv_compatibility_approximated =
            input.fdiv_compatibility_enabled && plan.vector_normalized;

        if (plan.fill_rectangle_emitted) {
            plan.ordered_operations.push_back({
                ScenePhysicsWheelBarOperationKind::fill_rectangle_0x44a1b0,
                plan.fill_left, plan.fill_top, plan.fill_right, plan.fill_bottom,
                plan.fill_color, 0U, 0.0});
        }
        plan.ordered_operations.push_back({
            ScenePhysicsWheelBarOperationKind::outline_circle_0x44c950,
            plan.x, plan.y, plan.outline_radius, 0, plan.frame_color, 0U, 0.0});
        plan.ordered_operations.push_back({
            ScenePhysicsWheelBarOperationKind::indicator_line_0x44c260,
            plan.x, plan.y, plan.indicator_end_x, plan.indicator_end_y,
            plan.indicator_color, 0U, 0.0});
        plan.ordered_operations.push_back({
            ScenePhysicsWheelBarOperationKind::formatted_text_0x479580,
            plan.x + 5, plan.y, 0x11, 0, 0U, 0x004D4C54U,
            plan.format_argument,
            plan.formatted_label_complete
                ? std::string(formatted_label.data(),
                              static_cast<std::size_t>(formatted_label_length))
                : std::string{}});
    }
    return plans;
}

ScenePhysicsWheelBarPixelWrites scene_physics_wheel_bar_pixel_writes(
    const ScenePhysicsWheelBarPlan& plan,
    const RenderContextViewport& context,
    const std::uint8_t renderer_mode,
    const std::uint32_t framebuffer_base)
{
    ScenePhysicsWheelBarPixelWrites result;
    if (plan.fill_rectangle_emitted) {
        result.fill_writes = scene_rectangle_fill_pixel_writes(
            plan.fill_left, plan.fill_top, plan.fill_right, plan.fill_bottom,
            plan.fill_color, context, renderer_mode, framebuffer_base);
    }

    std::int32_t circle_x = 0;
    auto circle_y = plan.outline_radius;
    auto decision = 1 - plan.outline_radius;
    const auto append_circle_pixel = [&](const std::int32_t px,
                                         const std::int32_t py) {
        auto writes = scene_rectangle_fill_pixel_writes(
            px, py, px + 1, py + 1, plan.frame_color, context,
            renderer_mode, framebuffer_base);
        result.outline_writes.insert(result.outline_writes.end(),
                                     writes.begin(), writes.end());
    };
    while (circle_x <= circle_y) {
        append_circle_pixel(plan.x + circle_x, plan.y + circle_y);
        append_circle_pixel(plan.x - circle_x, plan.y + circle_y);
        append_circle_pixel(plan.x + circle_x, plan.y - circle_y);
        append_circle_pixel(plan.x - circle_x, plan.y - circle_y);
        append_circle_pixel(plan.x + circle_y, plan.y + circle_x);
        append_circle_pixel(plan.x - circle_y, plan.y + circle_x);
        append_circle_pixel(plan.x + circle_y, plan.y - circle_x);
        append_circle_pixel(plan.x - circle_y, plan.y - circle_x);
        ++circle_x;
        if (decision < 0) {
            decision += 2 * circle_x + 1;
        } else {
            --circle_y;
            decision += 2 * (circle_x - circle_y) + 1;
        }
    }

    auto x = plan.x;
    auto y = plan.y;
    const auto delta_x = plan.indicator_end_x - plan.x;
    const auto delta_y = plan.indicator_end_y - plan.y;
    const auto x_step = delta_x < 0 ? -1 : 1;
    const auto y_step = delta_y < 0 ? -1 : 1;
    const auto append_pixel = [&](const std::int32_t px,
                                  const std::int32_t py) {
        auto writes = scene_rectangle_fill_pixel_writes(
            px, py, px + 1, py + 1, plan.indicator_color, context,
            renderer_mode, framebuffer_base);
        result.indicator_writes.insert(result.indicator_writes.end(),
                                       writes.begin(), writes.end());
    };
    if (delta_x == 0) {
        for (; y != plan.indicator_end_y; y += y_step) append_pixel(x, y);
    } else if (delta_y == 0) {
        for (; x != plan.indicator_end_x; x += x_step) append_pixel(x, y);
    } else {
        const auto abs_x = delta_x < 0 ? -static_cast<std::int64_t>(delta_x)
                                       : static_cast<std::int64_t>(delta_x);
        const auto abs_y = delta_y < 0 ? -static_cast<std::int64_t>(delta_y)
                                       : static_cast<std::int64_t>(delta_y);
        const auto round_like_ftol = [](const float value) {
            const auto rounded = std::nearbyint(static_cast<double>(value));
            if (rounded > static_cast<double>(std::numeric_limits<std::int32_t>::max()))
                return std::numeric_limits<std::int32_t>::max();
            if (rounded < static_cast<double>(std::numeric_limits<std::int32_t>::min()))
                return std::numeric_limits<std::int32_t>::min();
            return static_cast<std::int32_t>(rounded);
        };
        if (abs_x < abs_y) {
            const auto slope = static_cast<float>(
                static_cast<long double>(delta_x) / delta_y * y_step);
            auto current_x = static_cast<float>(plan.x);
            for (; y != plan.indicator_end_y; y += y_step) {
                append_pixel(round_like_ftol(current_x), y);
                current_x = static_cast<float>(current_x + slope);
            }
        } else {
            const auto slope = static_cast<float>(
                static_cast<long double>(delta_y) / delta_x * x_step);
            auto current_y = static_cast<float>(plan.y);
            for (; x != plan.indicator_end_x; x += x_step) {
                append_pixel(x, round_like_ftol(current_y));
                current_y = static_cast<float>(current_y + slope);
            }
        }
    }
    return result;
}

SceneRectangleFillApplyResult apply_scene_physics_wheel_bar_pixel_writes(
    const ScenePhysicsWheelBarPixelWrites& bar_writes,
    const std::span<std::uint8_t> framebuffer)
{
    std::vector<SceneRectangleFillPixelWrite> writes;
    writes.reserve(bar_writes.fill_writes.size() +
                   bar_writes.outline_writes.size() +
                   bar_writes.indicator_writes.size());
    writes.insert(writes.end(), bar_writes.fill_writes.begin(),
                  bar_writes.fill_writes.end());
    writes.insert(writes.end(), bar_writes.outline_writes.begin(),
                  bar_writes.outline_writes.end());
    writes.insert(writes.end(), bar_writes.indicator_writes.begin(),
                  bar_writes.indicator_writes.end());
    return apply_scene_rectangle_fill_pixel_writes(writes, framebuffer);
}

std::vector<SceneDashboardViewportCall> scene_dashboard_viewport_plan(
    const std::int32_t surface_width,
    const std::uint32_t dashboard_mask)
{
    struct Rectangle {
        std::int32_t left;
        std::int32_t top;
        std::int32_t right;
        std::int32_t bottom;
    };

    constexpr std::array<Rectangle, 12> rectangles{{
        {0x156, 0x1A, 0x1B0, 0x25},
        {0x032, 0x19, 0x0C8, 0x32},
        {0x1FE, 0x23, 0x22C, 0x2F},
        {0x010, 0x0E, 0x033, 0x31},
        {0x04C, 0x1E, 0x095, 0x31},
        {0x124, 0x0D, 0x14A, 0x13},
        {0x19A, 0x0D, 0x1B4, 0x13},
        {0x0DE, 0x2C, 0x137, 0x33},
        {0x234, 0x1E, 0x257, 0x26},
        {0x230, 0x27, 0x24E, 0x2F},
        {0x0E6, 0x1A, 0x152, 0x24},
        {0x156, 0x2C, 0x1AF, 0x33},
    }};

    auto horizontal_offset = (surface_width - 0x280) / 2;
    if (horizontal_offset < 0 &&
        (surface_width == 0x200 || surface_width == 0x140)) {
        horizontal_offset = -0xC0;
    }

    std::vector<SceneDashboardViewportCall> calls;
    calls.reserve(12);

    const auto append_rectangle = [&](const std::size_t index,
                                      const std::uint32_t required_mask_bit) {
        const auto& rectangle = rectangles[index];
        const auto left = (rectangle.left + horizontal_offset) & ~3;
        const auto right = (rectangle.right + horizontal_offset + 3) & ~3;
        const auto row_source = index < 3
            ? 0x004CF9F0U + static_cast<std::uint32_t>(index) * 0x10U
            : 0x004CFA20U + static_cast<std::uint32_t>(index - 3) * 0x10U;
        calls.push_back({left, rectangle.top, right, rectangle.bottom,
                         row_source, required_mask_bit});
    };

    for (std::size_t index = 0; index < 3; ++index) {
        append_rectangle(index, 0);
    }
    for (std::size_t index = 3; index < rectangles.size(); ++index) {
        const auto mask_bit = std::uint32_t{1} << (index - 3);
        if ((dashboard_mask & mask_bit) != 0) {
            append_rectangle(index, mask_bit);
        }
    }
    return calls;
}

std::vector<SceneDashboardRenderOperation> scene_dashboard_render_plan(
    const SceneDashboardRenderObservation& observation)
{
    auto horizontal_offset = (observation.surface_width - 0x280) / 2;
    if (horizontal_offset < 0 &&
        (observation.surface_width == 0x200 ||
         observation.surface_width == 0x140)) {
        horizontal_offset = -0xC0;
    }

    const auto viewports = scene_dashboard_viewport_plan(
        observation.surface_width, observation.dashboard_mask);
    std::vector<SceneDashboardRenderOperation> operations;
    operations.reserve(viewports.size() * 2U + 3U);

    const auto append_bitmap = [&]() {
        operations.push_back({
            SceneDashboardRenderOperationKind::bitmap_draw_0x44afd0,
            horizontal_offset,
            0,
            observation.background_bitmap_width,
            observation.background_bitmap_height,
            0,
            0,
            0,
            0x004EB3D0,
            observation.background_bitmap_width,
            observation.background_bitmap_height,
            observation.renderer_mode_0x4d7a48,
        });
    };

    if (observation.dashboard_mask == 0xFFFFU) {
        if (horizontal_offset != 0) {
            operations.push_back({
                SceneDashboardRenderOperationKind::full_surface_fill_0x44a1b0,
                0,
                0,
                observation.surface_width,
                observation.full_surface_bottom_0x4eb3dc,
                0x004EB3DC,
                0,
                observation.full_surface_fill_argument_0x4eb27c,
                0,
                0,
                0,
                0,
            });
        }
        append_bitmap();
    }

    for (const auto& viewport : viewports) {
        operations.push_back({
            SceneDashboardRenderOperationKind::viewport_0x4498a0,
            viewport.left,
            viewport.top,
            viewport.right,
            viewport.bottom,
            viewport.row_source,
            viewport.required_mask_bit,
            0,
            0,
            0,
            0,
            0,
        });
        append_bitmap();
    }
    operations.push_back({
        SceneDashboardRenderOperationKind::restore_viewport_0x4498e0,
    });
    return operations;
}

SceneDashboardBackgroundBitmapBlitResult
scene_dashboard_background_bitmap_blit_plan(
    const SceneDashboardBackgroundBitmapBlitObservation& observation) noexcept
{
    SceneDashboardBackgroundBitmapBlitResult result{};
    const auto source_mode = observation.source_bitmap_mode;
    const auto destination_mode = observation.destination_renderer_mode;
    if (source_mode == 5U && destination_mode == 5U) {
        result.path = SceneDashboardBitmapBlitPath::copy_32bit;
    } else if (source_mode != 5U && destination_mode != 5U) {
        if (source_mode == destination_mode) {
            result.path = SceneDashboardBitmapBlitPath::copy_16bit;
        } else if (source_mode == 4U && destination_mode == 3U) {
            result.path = SceneDashboardBitmapBlitPath::convert_mode4_to_mode3;
        }
    } else if (destination_mode == 5U) {
        if (source_mode == 3U) {
            result.path = SceneDashboardBitmapBlitPath::convert_mode3_to_mode5;
        } else if (source_mode == 4U) {
            result.path = SceneDashboardBitmapBlitPath::convert_mode4_to_mode5;
        }
    } else if (destination_mode == 3U) {
        result.path = SceneDashboardBitmapBlitPath::convert_mode5_to_mode3;
    } else if (destination_mode == 4U) {
        result.path = SceneDashboardBitmapBlitPath::convert_mode5_to_mode4;
    }

    result.width = observation.bitmap_width;
    result.height = observation.bitmap_height;
    result.destination_x = observation.destination_x;
    result.destination_y = observation.destination_y;

    const auto& context = observation.destination_context;
    const auto right = context.viewport_right_0x1c;
    const auto bottom = context.viewport_bottom_0x20;
    const auto destination_right = static_cast<std::int64_t>(
        result.destination_x) + result.width;
    const auto destination_bottom = static_cast<std::int64_t>(
        result.destination_y) + result.height;
    if (result.width < 0 || result.height < 0 ||
        destination_right < context.viewport_left_0x14 ||
        destination_bottom < context.viewport_top_0x18 ||
        right <= result.destination_x || bottom <= result.destination_y) {
        return result;
    }

    if (result.destination_x < context.viewport_left_0x14) {
        const auto clipped = context.viewport_left_0x14 - result.destination_x;
        result.source_x += clipped;
        result.width -= clipped;
        result.destination_x = context.viewport_left_0x14;
    }
    if (result.destination_y < context.viewport_top_0x18) {
        const auto clipped = context.viewport_top_0x18 - result.destination_y;
        result.source_y += clipped;
        result.height -= clipped;
        result.destination_y = context.viewport_top_0x18;
    }
    if (right < static_cast<std::int64_t>(result.destination_x) + result.width) {
        result.width = right - result.destination_x;
    }
    if (bottom < static_cast<std::int64_t>(result.destination_y) + result.height) {
        result.height = bottom - result.destination_y;
    }
    if (result.width < 1 || result.height < 1) {
        return result;
    }

    result.transfer_happened =
        result.path != SceneDashboardBitmapBlitPath::unsupported_format_pair;
    const auto source_bytes_per_pixel = source_mode == 5U ? 4U : 2U;
    const auto destination_bytes_per_pixel = destination_mode == 5U ? 4U : 2U;
    result.source_offset =
        static_cast<std::uint32_t>(result.source_y) *
            static_cast<std::uint32_t>(observation.bitmap_pitch_bytes) +
        static_cast<std::uint32_t>(result.source_x) * source_bytes_per_pixel;
    result.source_address = observation.bitmap_base_address + result.source_offset;
    result.destination_offset =
        static_cast<std::uint32_t>(result.destination_y) *
            static_cast<std::uint32_t>(context.row_pitch_0x10) +
        static_cast<std::uint32_t>(result.destination_x) *
            destination_bytes_per_pixel;
    result.destination_address = observation.destination_base_address +
        result.destination_offset;
    return result;
}

SceneDashboardBitmapPixelConversion scene_dashboard_bitmap_pixel_conversion(
    const std::uint8_t source_mode,
    const std::uint8_t destination_mode,
    const std::uint32_t source_pixel) noexcept
{
    SceneDashboardBitmapPixelConversion result{};
    if (source_mode == 5U && destination_mode == 5U) {
        result = {true, source_pixel, 4};
    } else if (source_mode != 5U && destination_mode != 5U) {
        if (source_mode == destination_mode) {
            result = {true, source_pixel & 0xFFFFU, 2};
        } else if (source_mode == 4U && destination_mode == 3U) {
            const auto pixel = source_pixel & 0xFFFFU;
            result = {true, (pixel & 0x001FU) | ((pixel & 0xFFC0U) >> 1), 2};
        }
    } else if (destination_mode == 5U &&
               (source_mode == 3U || source_mode == 4U)) {
        const auto pixel = static_cast<std::uint16_t>(source_pixel);
        const auto high_shift = source_mode == 3U ? 3U : 2U;
        const auto high = static_cast<std::uint8_t>(
            static_cast<std::uint8_t>(pixel >> 5) << high_shift);
        const auto low = static_cast<std::uint8_t>(
            static_cast<std::uint8_t>(pixel) << 3);
        result = {true, (static_cast<std::uint32_t>(high) << 8) | low, 4};
    } else if (source_mode == 5U &&
               (destination_mode == 3U || destination_mode == 4U)) {
        const auto a = (source_pixel >> 16) & 0xFFU;
        const auto b = (source_pixel >> 8) & 0xFFU;
        const auto c = source_pixel & 0xFFU;
        std::uint32_t pixel{};
        if (destination_mode == 3U) {
            pixel = ((a & 0xF8U) << 8) | ((b & 0xF8U) << 3) | (c >> 3);
        } else {
            pixel = (((a & 0xF8U) << 5) | (b & 0xF8U)) << 2 | (c >> 3);
        }
        result = {true, pixel & 0xFFFFU, 2};
    }
    return result;
}

std::vector<SceneDashboardBitmapPixelWrite>
scene_dashboard_background_bitmap_pixel_writes(
    const SceneDashboardBackgroundBitmapBlitObservation& observation,
    const std::span<const std::uint8_t> source_pixels)
{
    std::vector<SceneDashboardBitmapPixelWrite> writes;
    const auto plan = scene_dashboard_background_bitmap_blit_plan(observation);
    if (!plan.transfer_happened || plan.width <= 0 || plan.height <= 0 ||
        observation.bitmap_pitch_bytes <= 0 ||
        observation.destination_context.row_pitch_0x10 <= 0 ||
        plan.source_x < 0 || plan.source_y < 0 || plan.destination_x < 0 ||
        plan.destination_y < 0) {
        return writes;
    }

    const auto source_bytes_per_pixel = observation.source_bitmap_mode == 5U
        ? std::uint64_t{4}
        : std::uint64_t{2};
    const auto destination_bytes_per_pixel =
        observation.destination_renderer_mode == 5U
            ? std::uint64_t{4}
            : std::uint64_t{2};
    const auto pixel_count = static_cast<std::uint64_t>(plan.width) *
        static_cast<std::uint64_t>(plan.height);
    if (pixel_count > std::numeric_limits<std::size_t>::max()) {
        return writes;
    }
    writes.reserve(static_cast<std::size_t>(pixel_count));

    for (std::int32_t row = 0; row < plan.height; ++row) {
        const auto source_row = static_cast<std::uint64_t>(plan.source_y) +
            static_cast<std::uint64_t>(row);
        const auto destination_row =
            static_cast<std::uint64_t>(plan.destination_y) +
            static_cast<std::uint64_t>(row);
        for (std::int32_t column = 0; column < plan.width; ++column) {
            const auto source_column = static_cast<std::uint64_t>(plan.source_x) +
                static_cast<std::uint64_t>(column);
            const auto source_offset = source_row *
                static_cast<std::uint64_t>(observation.bitmap_pitch_bytes) +
                source_column * source_bytes_per_pixel;
            if (source_offset > source_pixels.size() ||
                source_bytes_per_pixel > source_pixels.size() - source_offset) {
                writes.clear();
                return writes;
            }
            std::uint32_t source_pixel = source_pixels[
                static_cast<std::size_t>(source_offset)];
            source_pixel |= static_cast<std::uint32_t>(source_pixels[
                static_cast<std::size_t>(source_offset + 1U)]) << 8;
            if (source_bytes_per_pixel == 4U) {
                source_pixel |= static_cast<std::uint32_t>(source_pixels[
                    static_cast<std::size_t>(source_offset + 2U)]) << 16;
                source_pixel |= static_cast<std::uint32_t>(source_pixels[
                    static_cast<std::size_t>(source_offset + 3U)]) << 24;
            }
            const auto converted = scene_dashboard_bitmap_pixel_conversion(
                observation.source_bitmap_mode,
                observation.destination_renderer_mode, source_pixel);
            if (!converted.supported) {
                writes.clear();
                return writes;
            }

            const auto destination_offset = destination_row *
                static_cast<std::uint64_t>(
                    observation.destination_context.row_pitch_0x10) +
                (static_cast<std::uint64_t>(plan.destination_x) +
                 static_cast<std::uint64_t>(column)) *
                    destination_bytes_per_pixel;
            writes.push_back({
                static_cast<std::uint32_t>(source_offset),
                observation.bitmap_base_address +
                    static_cast<std::uint32_t>(source_offset),
                static_cast<std::uint32_t>(destination_offset),
                observation.destination_base_address +
                    static_cast<std::uint32_t>(destination_offset),
                converted.value,
                converted.destination_bytes_per_pixel,
            });
        }
    }
    return writes;
}

SceneDashboardBitmapBlitApplyResult
apply_scene_dashboard_background_bitmap_blit(
    const SceneDashboardBackgroundBitmapBlitObservation& observation,
    const std::span<const std::uint8_t> source_pixels,
    const std::span<std::uint8_t> destination_pixels)
{
    const auto plan = scene_dashboard_background_bitmap_blit_plan(observation);
    if (!plan.transfer_happened) {
        return {};
    }
    const auto writes = scene_dashboard_background_bitmap_pixel_writes(
        observation, source_pixels);
    if (writes.empty()) {
        return {};
    }
    for (const auto& write : writes) {
        const auto offset = static_cast<std::uint64_t>(write.destination_offset);
        const auto bytes = static_cast<std::uint64_t>(write.bytes_per_pixel);
        if (offset > destination_pixels.size() ||
            bytes > destination_pixels.size() - offset) {
            return {};
        }
    }

    for (const auto& write : writes) {
        const auto offset = static_cast<std::size_t>(write.destination_offset);
        for (std::uint8_t byte = 0; byte < write.bytes_per_pixel; ++byte) {
            destination_pixels[offset + byte] = static_cast<std::uint8_t>(
                write.value >> (static_cast<unsigned>(byte) * 8U));
        }
    }
    return {true, writes.size()};
}

std::optional<SceneDashboardRectangleFillProjection>
scene_dashboard_full_surface_fill_projection(
    const SceneDashboardRenderObservation& observation)
{
    auto horizontal_offset = (observation.surface_width - 0x280) / 2;
    if (horizontal_offset < 0 &&
        (observation.surface_width == 0x200 ||
         observation.surface_width == 0x140)) {
        horizontal_offset = -0xC0;
    }
    if (observation.dashboard_mask != 0xFFFFU || horizontal_offset == 0) {
        return std::nullopt;
    }

    const auto mode = static_cast<std::uint8_t>(
        observation.renderer_mode_0x4d7a48);
    const auto source_color = static_cast<std::uint32_t>(
        observation.full_surface_fill_argument_0x4eb27c);
    return project_rectangle_fill(
        0,
        0,
        observation.surface_width,
        observation.full_surface_bottom_0x4eb3dc,
        source_color,
        observation.render_context,
        mode,
        observation.framebuffer_base_value);
}

std::vector<SceneRectangleFillPixelWrite> scene_rectangle_fill_pixel_writes(
    const std::int32_t left, const std::int32_t top,
    const std::int32_t right, const std::int32_t bottom,
    const std::uint32_t source_color,
    const RenderContextViewport& context,
    const std::uint8_t renderer_mode,
    const std::uint32_t framebuffer_base)
{
    std::vector<SceneRectangleFillPixelWrite> writes;
    const auto projection = project_rectangle_fill(
        left, top, right, bottom, source_color, context, renderer_mode,
        framebuffer_base);
    if (!projection.has_value()) {
        return writes;
    }
    const auto width = static_cast<std::size_t>(
        projection->clipped_right - projection->clipped_left);
    const auto height = static_cast<std::size_t>(
        projection->clipped_bottom - projection->clipped_top);
    writes.reserve(width * height);
    for (auto y = projection->clipped_top;
         y < projection->clipped_bottom; ++y) {
        for (auto x = projection->clipped_left;
             x < projection->clipped_right; ++x) {
            const auto offset = render_context_pixel_offset(
                context, renderer_mode, x, y);
            writes.push_back({
                x, y, offset, framebuffer_base + offset,
                projection->encoded_value, projection->write_mask,
                projection->bytes_per_pixel,
                projection->preserves_existing_bits});
        }
    }
    return writes;
}

SceneRectangleFillApplyResult apply_scene_rectangle_fill_pixel_writes(
    const std::span<const SceneRectangleFillPixelWrite> writes,
    const std::span<std::uint8_t> framebuffer)
{
    for (const auto& write : writes) {
        const auto offset = static_cast<std::size_t>(write.destination_offset);
        if (write.bytes_per_pixel == 0U || write.bytes_per_pixel > 4U ||
            offset > framebuffer.size() ||
            write.bytes_per_pixel > framebuffer.size() - offset) {
            return {};
        }
    }

    for (const auto& write : writes) {
        const auto offset = static_cast<std::size_t>(write.destination_offset);
        if (write.preserves_existing_bits) {
            const auto existing = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(framebuffer[offset]) |
                (static_cast<std::uint16_t>(framebuffer[offset + 1U]) << 8U));
            const auto updated = static_cast<std::uint16_t>(
                (existing & static_cast<std::uint16_t>(~write.write_mask)) |
                (write.value & write.write_mask));
            framebuffer[offset] = static_cast<std::uint8_t>(updated & 0xffU);
            framebuffer[offset + 1U] = static_cast<std::uint8_t>(updated >> 8U);
        } else {
            for (std::uint8_t byte = 0; byte < write.bytes_per_pixel; ++byte) {
                framebuffer[offset + byte] = static_cast<std::uint8_t>(
                    write.value >> (static_cast<std::uint32_t>(byte) * 8U));
            }
        }
    }
    return {true, writes.size()};
}

SceneDashboardTextWidthResult scene_dashboard_measure_text_width(
    const SceneDashboardTextWidthInput& input) noexcept
{
    const auto terminator = std::find(
        input.text_bytes.begin(), input.text_bytes.end(), std::uint8_t{0U});
    if (terminator == input.text_bytes.end()) {
        return {};
    }

    std::uint32_t width = 0U;
    const auto length = static_cast<std::size_t>(
        terminator - input.text_bytes.begin());
    if (input.font_object_present) {
        for (std::size_t index = 0; index < length; ++index) {
            const auto glyph = input.text_bytes[index];
            width += static_cast<std::uint32_t>(
                static_cast<std::int32_t>(input.glyph_advance_0x204[glyph]));
        }
    } else {
        width = static_cast<std::uint32_t>(length) * 8U;
    }
    return {
        std::bit_cast<std::int32_t>(width),
        true,
        !input.font_object_present,
    };
}

SceneDashboardFontExtentResult scene_dashboard_measure_font_extent(
    const std::uint32_t font_object_pointer,
    const SceneDashboardFontExtentInput& input) noexcept
{
    if (font_object_pointer == 0U) {
        return {8, true, true};
    }
    if (!input.payload_pointer_0x00.has_value()) {
        return {};
    }
    if (*input.payload_pointer_0x00 == 0U) {
        return {0, true, false};
    }
    if (!input.payload_metric_0x04.has_value()) {
        return {};
    }
    return {*input.payload_metric_0x04, true, false};
}

SceneDashboardCallbackGeometryPlan
scene_dashboard_callback_geometry_plan(
    const SceneDashboardCallbackGeometryObservation& observation)
{
    SceneDashboardCallbackGeometryPlan result{};
    result.copied_bytes_0x4eb2f8 = observation.source_bytes_0x4eb318;
    result.overlay_content_suppressed =
        result.copied_bytes_0x4eb2f8[0] != 0 ||
        observation.splash_loading_flag_0x4eb1e8;
    auto& operations = result.operations;
    bool batch_started = false;
    const auto begin_batch = [&]() {
        if (!batch_started) {
            operations.push_back({
                SceneDashboardCallbackGeometryKind::begin_batch_0x448460,
                0x00448460,
            });
            batch_started = true;
        }
    };
    const auto add_i32 = [](const std::int32_t left,
                            const std::int32_t right) noexcept {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) +
            static_cast<std::uint32_t>(right));
    };
    const auto sub_i32 = [](const std::int32_t left,
                            const std::int32_t right) noexcept {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) -
            static_cast<std::uint32_t>(right));
    };
    const auto signed_half = [](const std::int32_t value) noexcept {
        return value / 2;
    };
    const auto rectangle = [&](const std::array<std::int32_t, 4>& arguments) {
        operations.push_back({
            SceneDashboardCallbackGeometryKind::rectangle_0x4484c0,
            0x004484C0,
            arguments,
            0x7F000000,
        });
        SceneDashboardCallbackGeometryOperation::IndexedSubmission submission{};
        const auto vertex = [&](const std::int32_t x,
                                const std::int32_t y) {
            SceneDashboardCallbackGeometryOperation::Vertex value{};
            value.position = {
                static_cast<float>(x), static_cast<float>(y), 1.0F, 0.0F};
            value.diffuse_color = 0x7F000000U;
            return value;
        };
        submission.vertices = {
            vertex(arguments[0], arguments[1]),
            vertex(arguments[2], arguments[1]),
            vertex(arguments[0], arguments[3]),
            vertex(arguments[2], arguments[3]),
        };
        operations.back().indexed_submission = submission;
    };
    const auto refresh_resource = [&](const std::uint32_t resource_object) {
        operations.push_back({
            SceneDashboardCallbackGeometryKind::refresh_resource_0x418cc0,
            0x00418CC0,
            {},
            0,
            resource_object,
        });
    };
    const auto measure_text = [&](const std::uint32_t text_handle) {
        operations.push_back({
            SceneDashboardCallbackGeometryKind::measure_text_0x44db10,
            0x0044DB10,
            {},
            0,
            0,
            {observation.font_object_value_0x4eb304, text_handle},
            2,
        });
        operations.push_back({
            SceneDashboardCallbackGeometryKind::measure_font_0x44dc50,
            0x0044DC50,
            {},
            0,
            0,
            {observation.font_object_value_0x4eb304, 0},
            1,
        });
        if (!result.measured_font_extent.has_value() &&
            observation.font_extent_input.has_value()) {
            result.measured_font_extent = scene_dashboard_measure_font_extent(
                observation.font_object_value_0x4eb304,
                *observation.font_extent_input);
        }
    };

    if (observation.source_bytes_0x4eb318[3] != 0) {
        const auto scaled = std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(
                observation.resource_0x4eb46c_metric_00) * 130U);
        const auto margin = scaled / 100;
        const auto first_axis = sub_i32(
            signed_half(observation.surface_metric_0x509a2c), 0x8C);
        const auto second_axis = signed_half(sub_i32(
            observation.surface_metric_0x509a4c, margin));
        const auto second_axis_end = add_i32(second_axis, margin);
        const auto first_axis_end = add_i32(
            add_i32(first_axis, observation.resource_0x4eb46c_metric_04),
            0x14);
        begin_batch();
        rectangle({second_axis, first_axis, second_axis_end, first_axis_end});
    }

    if (result.copied_bytes_0x4eb2f8[0] != 0) {
        const auto first = signed_half(sub_i32(
            observation.surface_metric_0x509a4c,
            observation.resource_0x4eb448_metric_00));
        const auto second = signed_half(sub_i32(
            observation.surface_metric_0x509a2c,
            observation.resource_0x4eb448_metric_04));
        begin_batch();
        rectangle({
            first,
            second,
            add_i32(first, observation.resource_0x4eb448_metric_00),
            add_i32(second, observation.resource_0x4eb448_metric_04),
        });
    }

    operations.push_back({
        SceneDashboardCallbackGeometryKind::check_overlay_suppression_0x404890,
        0x00404890,
    });
    if (!result.overlay_content_suppressed &&
        result.copied_bytes_0x4eb2f8[1] != 0) {
        if (!observation.resource_0x4eb290_generation_matches) {
            refresh_resource(0x004EB290);
        }
        measure_text(observation.text_handle_value_0x4eb294);
        auto text_width = observation.flag_0x4eb294_text_width;
        if (observation.flag_0x4eb294_text_input.has_value()) {
            result.flag_0x4eb294_measured_width =
                scene_dashboard_measure_text_width(
                    *observation.flag_0x4eb294_text_input);
            if (result.flag_0x4eb294_measured_width->complete) {
                text_width = result.flag_0x4eb294_measured_width->width;
            }
        }
        text_width = add_i32(text_width, 0x14);
        const auto measured_extent = result.measured_font_extent.has_value() &&
                result.measured_font_extent->complete
            ? result.measured_font_extent->extent
            : observation.font_metric_0x44dc50;
        const auto font_extent = add_i32(measured_extent, 0x48);
        begin_batch();
        rectangle({0, 0x40, text_width, font_extent});
    }

    operations.push_back({
        SceneDashboardCallbackGeometryKind::check_overlay_suppression_0x404890,
        0x00404890,
    });
    if (!result.overlay_content_suppressed &&
        result.copied_bytes_0x4eb2f8[2] != 0) {
        if (!observation.resource_0x4eb428_generation_matches) {
            refresh_resource(0x004EB428);
        }
        measure_text(observation.text_handle_value_0x4eb42c);
        auto text_width = observation.flag_0x4eb42c_text_width;
        if (observation.flag_0x4eb42c_text_input.has_value()) {
            result.flag_0x4eb42c_measured_width =
                scene_dashboard_measure_text_width(
                    *observation.flag_0x4eb42c_text_input);
            if (result.flag_0x4eb42c_measured_width->complete) {
                text_width = result.flag_0x4eb42c_measured_width->width;
            }
        }
        text_width = add_i32(text_width, 0x14);
        const auto measured_extent = result.measured_font_extent.has_value() &&
                result.measured_font_extent->complete
            ? result.measured_font_extent->extent
            : observation.font_metric_0x44dc50;
        const auto font_extent = add_i32(measured_extent, 0x48);
        const auto left = sub_i32(sub_i32(
            observation.surface_metric_0x509a4c, text_width), 1);
        begin_batch();
        rectangle({left, 0x40, text_width, font_extent});
    }

    if (batch_started) {
        operations.push_back({
            SceneDashboardCallbackGeometryKind::finish_batch_0x448490,
            0x00448490,
        });
    }
    return result;
}

SceneDashboardInitializationPlan scene_dashboard_initialization_plan()
{
    using Kind = SceneDashboardInitializationOperationKind;
    SceneDashboardInitializationPlan plan{};
    plan.reset_state.reset_byte_0x4eb300 = true;
    const auto op = [&plan](const Kind kind, const std::uint32_t helper,
                            const std::uint32_t destination = 0U,
                            const std::uint32_t key = 0U,
                            const std::uint32_t first = 0U,
                            const std::uint32_t second = 0U,
                            const std::uint8_t value_size = 0U) {
        plan.ordered_operations.push_back(
            {kind, helper, destination, key, first, second, value_size});
    };
    op(Kind::store_global, 0U, 0x004EB2F8U, 0U, 0U, 0U, 4U);
    op(Kind::store_global, 0U, 0x004EB368U, 0U, 0U, 0U, 4U);
    op(Kind::store_global, 0U, 0x004EB2FCU, 0U, 0U, 0U, 2U);
    op(Kind::store_global, 0U, 0x004CF9C4U, 0U, 0U, 0U, 4U);
    op(Kind::store_global, 0U, 0x004CF9C0U, 0U, 0U, 0U, 4U);
    op(Kind::store_global, 0x00413BD0U, 0x004EB244U, 0U, 0U, 0U, 4U);
    op(Kind::store_global, 0U, 0x004EB398U, 0U, 0U, 0U, 4U);
    op(Kind::initialize_text_resource, 0x0046B690U, 0x004EB278U,
       0x004CFB98U);
    op(Kind::initialize_text_resource, 0x0046B690U, 0x004EB3F8U,
       0x004CFBA8U);
    op(Kind::initialize_text_resource, 0x0046B690U, 0x004EB380U,
       0x004CFBB8U);
    for (const auto [key, destination] : {
             std::pair{0x004CFBC8U, 0x004EB46CU},
             std::pair{0x004CFBD4U, 0x004EB324U},
             std::pair{0x004CFBDCU, 0x004EB420U},
             std::pair{0x004CFBE4U, 0x004EB48CU},
             std::pair{0x004CFBF0U, 0x004EB320U},
             std::pair{0x004CFBFCU, 0x004EB350U}}) {
        op(Kind::register_resource, 0x0044CFD0U, destination, key);
    }
    for (const auto [key, destination] : {
             std::pair{0x004CFC08U, 0x004EB2A8U},
             std::pair{0x004CFC14U, 0x004EB304U},
             std::pair{0x004CFC20U, 0x004EB378U},
             std::pair{0x004CFC30U, 0x004EB28CU},
             std::pair{0x004CFC3CU, 0x004EB41CU}}) {
        op(Kind::register_resource, 0x0044DA00U, destination, key);
    }
    for (std::uint32_t index = 0; index < 2U; ++index) {
        op(Kind::measure_text, 0x0044D460U, 0x004EB48CU);
        op(Kind::measure_text, 0x0044D450U, 0x004EB48CU);
        op(Kind::initialize_text_context, 0x00449390,
           index == 0U ? 0x004EB3D0U : 0x004EB2C0U);
    }
    op(Kind::set_text_context, 0x00449900U, 0x004EB2C0U);
    op(Kind::reset_text_context, 0x00449AA0U, 0x004EB27CU);
    op(Kind::draw_text, 0x0044D0C0U, 0x004EB48CU);
    for (const auto [destination, first, second] : {
             std::tuple{0x004EB314U, 0xFFE67FE6U, 0x18E30C63U},
             std::tuple{0x004EB3F4U, 0x001F001FU, 0x18E30C63U},
             std::tuple{0x004EB488U, 0xFFFF7FFFU, 0x18E30C63U},
             std::tuple{0x004EB34CU, 0xFFE07FE0U, 0x21041084U}}) {
        op(Kind::create_colored_resource, 0x0044D4D0U, destination);
        op(Kind::set_resource_color, 0x0044D530U, destination, 0U,
           first, second);
    }
    op(Kind::register_resource, 0x0044CFD0U, 0x004EB448U, 0x004CFC48U);
    for (const auto [destination, first, second] : {
             std::tuple{0x004EB370U, 0xFFFF7FFFU, 0x4A492529U},
             std::tuple{0x004EB374U, 0xFFE07FE0U, 0x4A492529U}}) {
        op(Kind::create_colored_resource, 0x0044D4D0U, destination);
        op(Kind::set_resource_color, 0x0044D530U, destination, 0U,
           first, second);
    }
    for (const auto global : {0x004CF9C8U, 0x004CF9CCU, 0x004CF9D0U,
                              0x004CF9D4U}) {
        op(Kind::store_global, 0U, global, 0U, 0xFFFFFFFFU, 0U, 4U);
    }
    for (const auto global : {0x004CF9D8U, 0x004CF9DCU, 0x004CF9E0U,
                              0x004CF9E4U}) {
        op(Kind::store_global, 0U, global, 0U, 0xBF800000U, 0U, 4U);
    }
    op(Kind::store_global, 0U, 0x004CF9E8U, 0U, 0xFFFFFFFFU, 0U, 4U);
    op(Kind::store_global, 0U, 0x004EB300U, 0U, 1U, 0U, 1U);
    op(Kind::store_global, 0U, 0x004CF9ECU, 0U, 0xFFFFFFFFU, 0U, 4U);
    return plan;
}

SceneDashboardGearDisplayResult scene_dashboard_gear_display_plan(
    const SceneDashboardGearDisplayObservation& observation)
{
    SceneDashboardGearDisplayResult result{};
    if (!observation.dashboard_path_enabled) {
        return result;
    }

    const auto register_resource = [&](const std::uint32_t bit,
                                       const SceneDashboardGearOperationKind kind) {
        if ((observation.resource_initialization_mask_0x4eb274 & bit) == 0) {
            result.newly_set_resource_bits |= bit;
            result.operations.push_back({kind});
        }
    };
    register_resource(0x01U,
                      SceneDashboardGearOperationKind::register_final_time_banner);

    if (observation.surface_width <= 0x27f) {
        return result;
    }
    register_resource(0x02U,
                      SceneDashboardGearOperationKind::register_reverse_character);
    register_resource(0x04U,
                      SceneDashboardGearOperationKind::register_neutral_character);
    if ((observation.resource_initialization_mask_0x4eb274 & 0x08U) == 0) {
        result.newly_set_resource_bits |= 0x08U;
        result.initialized_geometry_constants = true;
        result.operations.push_back({
            SceneDashboardGearOperationKind::initialize_gear_geometry_constants});
    }

    if (observation.mode_0x45c200 == 0) {
        result.operations.push_back({
            SceneDashboardGearOperationKind::fill_mode_zero_rectangle,
            0x14,
            observation.surface_height - 0x29,
            0x28,
            observation.surface_height - 0x0f,
            static_cast<std::uint32_t>(
                observation.mode_zero_fill_argument_0x4eb27c)});
        result.operations.push_back({SceneDashboardGearOperationKind::flush_text});
    } else if (observation.mode_0x45c200 != 0x0c) {
        result.operations.push_back({
            SceneDashboardGearOperationKind::draw_gear_resource,
            0x10,
            observation.surface_height - 0x90,
            0,
            0,
            observation.gear_resource_value_0x4eb420,
            0});
        result.operations.push_back({SceneDashboardGearOperationKind::flush_text});
        auto value = observation.selected_record_value_0x34;
        if (value < 500.0F) {
            value = 500.0F;
        }
        result.operations.push_back({
            SceneDashboardGearOperationKind::draw_rotating_gear_geometry,
            0x50,
            observation.surface_height - 0x50,
            0x32,
            0,
            0,
            0,
            value,
            -0.27925268F,
            0.00066322513F});

        const auto angle = static_cast<long double>(-0.27925268F) +
            static_cast<long double>(0.00066322513F) *
                static_cast<long double>(value);
        const auto sine = std::sin(angle);
        const auto cosine = std::cos(angle);
        const auto round_like_ftol = [](const long double coordinate) {
            return truncate_to_low_i32(std::nearbyint(coordinate));
        };
        const auto center_x = 0x50;
        const auto center_y = observation.surface_height - 0x50;
        const auto radius = static_cast<long double>(0x32);
        result.rotating_gear_geometry = SceneDashboardGearGeometry{
            static_cast<float>(angle),
            {center_x + round_like_ftol(-sine * radius),
             center_y + round_like_ftol(cosine * radius)},
            {center_x + round_like_ftol(3.0L * cosine),
             center_y + round_like_ftol(3.0L * sine)},
            {center_x - round_like_ftol(3.0L * cosine),
             center_y - round_like_ftol(3.0L * sine)},
            observation.triangle_color_value_0x4eb29c,
            observation.first_edge_color_value_0x4eb27c,
            observation.second_edge_color_value_0x4eb33c,
        };

        const auto& triangle = *result.rotating_gear_geometry;
        const std::array<SceneDashboardGearPoint, 3> vertices{
            triangle.tip,
            triangle.first_base_corner,
            triangle.second_base_corner,
        };
        auto minimum_y = vertices[0].y;
        auto maximum_y = vertices[0].y;
        for (const auto& vertex : vertices) {
            minimum_y = std::min(minimum_y, vertex.y);
            maximum_y = std::max(maximum_y, vertex.y);
        }

        // FUN_0044A230 reserves two 1024-entry DWORD edge arrays and emits
        // one 0x44A1B0 rectangle for each nonempty visible scanline.
        const auto first_visible_y = std::max(minimum_y, 0);
        const auto last_visible_y = std::min({
            maximum_y,
            observation.render_context.surface_height_0x0c,
            1024,
        });
        for (auto y = first_visible_y; y < last_visible_y; ++y) {
            std::array<std::int32_t, 3> crossings{};
            std::size_t crossing_count{};
            for (std::size_t edge = 0; edge < vertices.size(); ++edge) {
                const auto& start = vertices[edge];
                const auto& end = vertices[(edge + 1U) % vertices.size()];
                if (start.y == end.y) {
                    continue;
                }
                const auto low_y = std::min(start.y, end.y);
                const auto high_y = std::max(start.y, end.y);
                if (y < low_y || y >= high_y) {
                    continue;
                }
                const auto fraction =
                    (static_cast<long double>(y) - start.y) /
                    (static_cast<long double>(end.y) - start.y);
                const auto x = static_cast<long double>(start.x) + fraction *
                    (static_cast<long double>(end.x) - start.x);
                crossings[crossing_count++] = round_like_ftol(x);
            }
            if (crossing_count < 2U) {
                continue;
            }
            std::sort(crossings.begin(), crossings.begin() +
                      static_cast<std::ptrdiff_t>(crossing_count));
            const auto left = crossings.front();
            const auto right = crossings[crossing_count - 1U];
            if (left < right) {
                result.rotating_gear_triangle_spans.push_back({
                    left, y, right, triangle.triangle_color_value});
            }
        }

        const auto append_edge_pixels = [&](
            const SceneDashboardGearPoint& start,
            const SceneDashboardGearPoint& end,
            const std::uint8_t edge_index,
            const std::uint32_t color) {
            const auto delta_x = end.x - start.x;
            const auto delta_y = end.y - start.y;
            const auto x_step = delta_x < 0 ? -1 : 1;
            const auto y_step = delta_y < 0 ? -1 : 1;
            if (delta_x == 0) {
                for (auto y = start.y; y != end.y; y += y_step) {
                    result.rotating_gear_edge_pixels.push_back(
                        {edge_index, start.x, y, color});
                }
                return;
            }
            if (delta_y == 0) {
                for (auto x = start.x; x != end.x; x += x_step) {
                    result.rotating_gear_edge_pixels.push_back(
                        {edge_index, x, start.y, color});
                }
                return;
            }

            const auto abs_x = delta_x < 0 ? -static_cast<std::int64_t>(delta_x)
                                           : static_cast<std::int64_t>(delta_x);
            const auto abs_y = delta_y < 0 ? -static_cast<std::int64_t>(delta_y)
                                           : static_cast<std::int64_t>(delta_y);
            if (abs_x < abs_y) {
                const auto slope = static_cast<float>(
                    static_cast<long double>(delta_x) / delta_y * y_step);
                auto x = static_cast<float>(start.x);
                for (auto y = start.y; y != end.y; y += y_step) {
                    result.rotating_gear_edge_pixels.push_back({
                        edge_index, round_like_ftol(x), y, color});
                    x = static_cast<float>(x + slope);
                }
            } else {
                const auto slope = static_cast<float>(
                    static_cast<long double>(delta_y) / delta_x * x_step);
                auto y = static_cast<float>(start.y);
                for (auto x = start.x; x != end.x; x += x_step) {
                    result.rotating_gear_edge_pixels.push_back({
                        edge_index, x, round_like_ftol(y), color});
                    y = static_cast<float>(y + slope);
                }
            }
        };
        append_edge_pixels(triangle.tip, triangle.first_base_corner, 0,
                           triangle.first_edge_color_value);
        append_edge_pixels(triangle.tip, triangle.second_base_corner, 1,
                           triangle.second_edge_color_value);

        for (std::size_t call_index = 0;
             call_index < result.rotating_gear_edge_pixels.size();
             ++call_index) {
            const auto& pixel_call = result.rotating_gear_edge_pixels[call_index];
            if (!render_context_contains_pixel(observation.render_context,
                                               pixel_call.x, pixel_call.y)) {
                continue;
            }
            const auto encoding = encode_render_pixel(
                observation.renderer_mode_0x4d7a48,
                pixel_call.color_value);
            const auto offset = render_context_pixel_offset(
                observation.render_context,
                observation.renderer_mode_0x4d7a48,
                pixel_call.x,
                pixel_call.y);
            result.rotating_gear_pixel_writes.push_back({
                call_index,
                pixel_call.x,
                pixel_call.y,
                offset,
                observation.framebuffer_base_value + offset,
                encoding.bits,
                encoding.bytes_per_pixel,
            });
        }

        for (std::size_t span_index = 0;
             span_index < result.rotating_gear_triangle_spans.size();
             ++span_index) {
            const auto& span = result.rotating_gear_triangle_spans[span_index];
            const auto projection = project_rectangle_fill(
                span.x_begin,
                span.y,
                span.x_end,
                span.y + 1,
                span.color_value,
                observation.render_context,
                observation.renderer_mode_0x4d7a48,
                observation.framebuffer_base_value);
            if (!projection.has_value()) {
                continue;
            }
            for (auto y = projection->clipped_top;
                 y < projection->clipped_bottom; ++y) {
                for (auto x = projection->clipped_left;
                     x < projection->clipped_right; ++x) {
                    const auto offset = render_context_pixel_offset(
                        observation.render_context,
                        observation.renderer_mode_0x4d7a48,
                        x,
                        y);
                    result.rotating_gear_triangle_fill_writes.push_back({
                        span_index,
                        x,
                        y,
                        offset,
                        observation.framebuffer_base_value + offset,
                        projection->encoded_value,
                        projection->write_mask,
                        projection->bytes_per_pixel,
                        projection->preserves_existing_bits,
                    });
                }
            }
        }
    }
    return result;
}

SceneDashboardPositionResult update_scene_dashboard_position(
    SceneDashboardState& state,
    const SceneDashboardPositionObservation& observation) noexcept
{
    const bool redraw_all = observation.context_changed_0x44e3d0 ||
        state.reset_byte_0x4eb300 ||
        state.selected_ordinal_cache_0x4cf9c8 != observation.selected_ordinal;
    if (redraw_all) {
        state.selected_ordinal_cache_0x4cf9c8 = observation.selected_ordinal;
        state.redraw_mask_0x4eb40c = 0xFFFF;
        state.reset_byte_0x4eb300 = false;
    }

    const auto position = observation.record_value_0x140 > 1
        ? observation.record_value_0x140
        : 1;
    auto count_bits = observation.entity_count;
    if (observation.decrement_entity_count_0x40e6d0) {
        --count_bits;
    }
    // The native DWORD decrement wraps, but its following JLE tests the
    // resulting bits as a signed 32-bit count.
    const auto count = std::bit_cast<std::int32_t>(count_bits);

    std::uint8_t reached_calls{};
    if (redraw_all) {
        reached_calls |= 0x01;
    }
    if (redraw_all || state.position_cache_0x4cf9cc != position) {
        reached_calls |= 0x02;
        state.position_cache_0x4cf9cc = position;
        state.redraw_mask_0x4eb40c |= 0x08;
    }

    constexpr std::uint32_t position_and_race_time_resource_bits = 0x03U;
    const auto newly_registered_resource_bits =
        position_and_race_time_resource_bits &
        ~state.resource_init_mask_0x4eb484;
    state.resource_init_mask_0x4eb484 |=
        position_and_race_time_resource_bits;

    return {redraw_all, position, count, newly_registered_resource_bits,
            reached_calls};
}

SceneDashboardLapResult update_scene_dashboard_lap(
    SceneDashboardState& state,
    const SceneDashboardLapObservation& observation,
    const bool redraw_all) noexcept
{
    SceneDashboardLapResult result{};
    if (observation.selected_object_type_0x18 != 0 ||
        observation.global_method_0x40_result) {
        return result;
    }
    result.normal_lap_branch = true;
    if ((state.resource_init_mask_0x4eb484 & 0x04U) == 0) {
        state.resource_init_mask_0x4eb484 |= 0x04U;
        result.resource_registered = true;
    }

    const bool dashboard_kind_changed = state.dashboard_kind_cache_0x4cf9ec != 1;
    if (dashboard_kind_changed) {
        state.dashboard_kind_cache_0x4cf9ec = 1;
        state.redraw_mask_0x4eb40c |= 0x80;
        if (!redraw_all) {
            result.requested_dirty_rectangles |= 0x80;
        }
    }
    if (redraw_all || dashboard_kind_changed) {
        result.reached_text_call_sites |= 0x08;
    }

    const auto lap = observation.record_value_0x138 > 1
        ? observation.record_value_0x138
        : 1;
    result.displayed_lap = lap;
    result.displayed_lap_count = observation.global_method_0x20_result;
    if (redraw_all || state.lap_value_cache_0x4cf9d0 != lap) {
        result.reached_text_call_sites |= 0x10;
        state.lap_value_cache_0x4cf9d0 = lap;
        state.redraw_mask_0x4eb40c |= 0x04;
        if (!redraw_all) {
            result.requested_dirty_rectangles |= 0x04;
        }
    }
    return result;
}

SceneDashboardFinalTimeResult update_scene_dashboard_final_time(
    SceneDashboardState& state,
    const SceneDashboardFinalTimeObservation& observation,
    const bool redraw_all) noexcept
{
    SceneDashboardFinalTimeResult result{};
    if (observation.selected_object_type_0x18 != 0 ||
        !observation.global_method_0x40_result) {
        return result;
    }
    result.final_time_branch = true;

    const bool dashboard_kind_changed = state.dashboard_kind_cache_0x4cf9ec != 0;
    if (dashboard_kind_changed) {
        state.dashboard_kind_cache_0x4cf9ec = 0;
        state.redraw_mask_0x4eb40c |= 0x80;
        if (!redraw_all) {
            result.requested_dirty_rectangles |= 0x80;
        }
    }
    if (redraw_all || dashboard_kind_changed) {
        result.reached_text_call_sites |= 0x04;
    }
    // The formatted final-time value is submitted on every visit to this path.
    result.reached_text_call_sites |= 0x80;
    return result;
}

float scene_dashboard_time_for_ordinal(
    const std::span<const std::uint8_t> records,
    const std::size_t record_count,
    const std::uint8_t ordinal) noexcept
{
    constexpr std::size_t row_stride = 0x48;
    auto result = -1.0F;
    for (std::size_t index = 0; index < record_count; ++index) {
        if (index > std::numeric_limits<std::size_t>::max() / row_stride) {
            break;
        }
        const auto row_offset = index * row_stride;
        if (row_offset > records.size() || records.size() - row_offset < 0x42) {
            break;
        }
        const auto* const row = records.data() + row_offset;
        if (row[0x41] != ordinal) {
            continue;
        }
        const auto bits = static_cast<std::uint32_t>(row[0x0C]) |
            (static_cast<std::uint32_t>(row[0x0D]) << 8U) |
            (static_cast<std::uint32_t>(row[0x0E]) << 16U) |
            (static_cast<std::uint32_t>(row[0x0F]) << 24U);
        result = std::bit_cast<float>(bits);
        if ((row[0x40] & 0x20U) == 0) {
            break;
        }
    }
    return result;
}

std::string format_scene_dashboard_time(const float time_seconds,
                                        const char marker,
                                        const bool hundredths)
{
    const auto total_seconds = truncate_to_low_i32(time_seconds);
    const auto fractional_seconds = static_cast<long double>(time_seconds) -
        static_cast<long double>(total_seconds);
    const auto centiseconds = truncate_to_low_i32(fractional_seconds * 100.0L);
    const auto minutes = total_seconds / 60;
    const auto seconds = total_seconds - minutes * 60;

    std::array<char, 48> output{};
    if (minutes >= 60) {
        std::snprintf(output.data(), output.size(),
                      hundredths ? "99:99%c99" : "99:99%c9",
                      static_cast<int>(marker));
    } else if (hundredths) {
        std::snprintf(output.data(), output.size(), "%01d:%02d%c%02d",
                      minutes, seconds, static_cast<int>(marker), centiseconds);
    } else {
        std::snprintf(output.data(), output.size(), "%01d:%02d%c%01d",
                      minutes, seconds, static_cast<int>(marker), centiseconds / 10);
    }
    return std::string(output.data());
}

std::vector<SceneDashboardTextCall> scene_dashboard_text_plan(
    const std::uint8_t reached_call_site_mask)
{
    constexpr std::array<SceneDashboardTextCall, 8> call_sites{{
        {0x004036EA, 348, 18, 0x004CFC68, 0x004EB314, 0x004EB2A8, 0x21},
        {0x0040372E, 410, 18, 0x004CFC6C, 0x004EB314, 0x004EB2A8, 0x21},
        {0x00403820, 230, 35, 0x004CFC88, 0x004EB3F4, 0x004EB304, 0x21},
        {0x0040391C, 266, 18, 0x004CFCA0, 0x004EB314, 0x004EB2A8, 0x21},
        {0x00403972, 298, 18, 0x004CFCA4, 0x004EB314, 0x004EB2A8, 0x21},
        {0x00403A3F, 336, 35, 0x004CFCC0, 0, 0x004EB304, 0x22},
        {0x00403A66, 342, 35, 0x004CFCC4, 0, 0x004EB304, 0x21},
        {0x00403848, 342, 35, 0x004CFC8C, 0x004EB3F4, 0x004EB304, 0x21},
    }};

    std::vector<SceneDashboardTextCall> calls;
    calls.reserve(call_sites.size());
    for (std::size_t index = 0; index < call_sites.size(); ++index) {
        if ((reached_call_site_mask & (std::uint8_t{1} << index)) != 0) {
            calls.push_back(call_sites[index]);
        }
    }
    return calls;
}

SceneDashboardTextLayout layout_scene_dashboard_text(
    const SceneDashboardTextCall& call,
    const std::string_view formatted_text,
    const SceneDashboardFontObservation& font)
{
    const auto add_i32 = [](const std::int32_t left,
                            const std::int32_t right) noexcept {
        return std::bit_cast<std::int32_t>(
            std::bit_cast<std::uint32_t>(left) +
            std::bit_cast<std::uint32_t>(right));
    };
    const auto sub_i32 = [](const std::int32_t left,
                            const std::int32_t right) noexcept {
        return std::bit_cast<std::int32_t>(
            std::bit_cast<std::uint32_t>(left) -
            std::bit_cast<std::uint32_t>(right));
    };
    SceneDashboardTextLayout result{};
    for (const unsigned char character : formatted_text) {
        if (character == 0) {
            break;
        }
        result.measured_width = add_i32(
            result.measured_width,
            static_cast<std::int32_t>(font.glyph_advance[character]));
    }

    result.origin_x = call.x;
    if ((call.format_flags & 0x02U) != 0) {
        result.origin_x = sub_i32(result.origin_x, result.measured_width);
    } else if ((call.format_flags & 0x04U) != 0) {
        result.origin_x = sub_i32(result.origin_x, result.measured_width / 2);
    }

    result.origin_y = call.y;
    if ((call.format_flags & 0x08U) != 0) {
        result.origin_y = add_i32(result.origin_y, font.top_metric);
    } else if ((call.format_flags & 0x10U) != 0) {
        const auto centered_metric =
            sub_i32(font.top_metric, font.bottom_metric) / 2;
        result.origin_y = add_i32(result.origin_y, centered_metric);
    } else if ((call.format_flags & 0x40U) != 0) {
        result.origin_y = sub_i32(result.origin_y, font.bottom_metric);
    }

    auto glyph_x = result.origin_x;
    result.glyph_calls.reserve(formatted_text.size());
    for (const unsigned char character : formatted_text) {
        if (character == 0) {
            break;
        }
        const auto glyph = font.glyph_index[character];
        if (glyph >= 0) {
            result.glyph_calls.push_back({
                call.call_site,
                add_i32(glyph_x,
                    static_cast<std::int32_t>(font.glyph_x_offset[character])),
                result.origin_y,
                character,
                glyph,
            });
        }
        glyph_x = add_i32(glyph_x,
            static_cast<std::int32_t>(font.glyph_advance[character]));
    }
    return result;
}

SceneDashboardBuiltinFontPlan raster_scene_dashboard_builtin_font(
    const SceneDashboardBuiltinFontObservation& observation)
{
    SceneDashboardBuiltinFontPlan result{};
    const auto& renderer = observation.renderer;
    const auto bytes_per_pixel = renderer.renderer_mode == 5 ? 4 : 2;
    std::uint32_t pixel_value = 0;
    if (renderer.renderer_mode == 5) {
        const auto red = (((observation.color >> 19U) & 0xfcU) |
                          0xffff0000U) << 8U;
        const auto green = ((observation.color >> 24U) & 0xf8U) << 16U;
        const auto blue = (observation.color >> 13U) & 0xf8U;
        pixel_value = red | green | blue;
    } else if (renderer.renderer_mode == 3) {
        pixel_value = observation.color & 0xffffU;
    } else {
        pixel_value = observation.color >> 16U;
    }

    for (std::size_t character_index = 0;
         character_index < observation.text.size(); ++character_index) {
        const auto raw_character = static_cast<std::uint8_t>(
            observation.text[character_index]);
        if (raw_character == 0U) {
            break;
        }
        const auto signed_character = std::bit_cast<std::int8_t>(raw_character);
        const auto character = signed_character < 0x20
            ? static_cast<std::uint8_t>('#')
            : raw_character;
        if (signed_character < 0x20) {
            ++result.control_bytes_replaced;
        }
        const auto glyph_index = static_cast<std::size_t>(character - 0x20U);
        if (glyph_index >= detail::scene_font_8x8.size()) {
            result.input_complete = false;
            continue;
        }
        ++result.characters_drawn;
        const auto& bitmap = detail::scene_font_8x8[glyph_index];
        const auto glyph_x = static_cast<std::int64_t>(observation.x) +
            static_cast<std::int64_t>(character_index) * 8;
        for (std::int32_t row = 0; row < 8; ++row) {
            const auto row_bits = bitmap[static_cast<std::size_t>(row)];
            for (std::int32_t column = 0; column < 8; ++column) {
                if ((row_bits & (std::uint8_t{1} << column)) == 0) {
                    continue;
                }
                const auto pixel_x = glyph_x + column;
                const auto pixel_y = static_cast<std::int64_t>(observation.y) + row;
                if (pixel_x < renderer.clip_left ||
                    pixel_x >= renderer.clip_right ||
                    pixel_y < renderer.clip_top ||
                    pixel_y >= renderer.clip_bottom ||
                    pixel_x < std::numeric_limits<std::int32_t>::min() ||
                    pixel_x > std::numeric_limits<std::int32_t>::max() ||
                    pixel_y < std::numeric_limits<std::int32_t>::min() ||
                    pixel_y > std::numeric_limits<std::int32_t>::max()) {
                    continue;
                }
                const auto offset = pixel_y * renderer.framebuffer_pitch_bytes +
                    pixel_x * bytes_per_pixel;
                result.pixel_writes.push_back({
                    character_index,
                    static_cast<std::int32_t>(pixel_x),
                    static_cast<std::int32_t>(pixel_y),
                    offset,
                    static_cast<std::uint32_t>(
                        static_cast<std::int64_t>(renderer.framebuffer_base) + offset),
                    pixel_value,
                    static_cast<std::uint8_t>(bytes_per_pixel),
                });
            }
        }
    }
    return result;
}

SceneDashboardRasterPlan raster_scene_dashboard_text(
    const SceneDashboardTextLayout& layout,
    const SceneDashboardAtlasFontObservation& font,
    const SceneDashboardRasterObservation& renderer)
{
    SceneDashboardRasterPlan result{};
    const auto& palette = renderer.explicit_palette.empty()
        ? font.embedded_palette
        : renderer.explicit_palette;
    // Native palette pointers are int* (four-byte entries) but the pixel
    // lookup indexes uint16_t values, so +0x80 means 256 palette entries.
    const std::size_t palette_base = renderer.renderer_mode == 3 ? 0U : 256U;

    for (std::size_t glyph_call_index = 0;
         glyph_call_index < layout.glyph_calls.size(); ++glyph_call_index) {
        const auto& glyph = layout.glyph_calls[glyph_call_index];
        if (glyph.glyph_index < 0 ||
            static_cast<std::size_t>(glyph.glyph_index) >= font.glyph_count) {
            continue;
        }
        ++result.glyphs_considered;
        const auto glyph_index = static_cast<std::size_t>(glyph.glyph_index);
        if (glyph_index >= font.encoded_rows.size()) {
            result.input_complete = false;
            result.glyph_rows_missing += static_cast<std::size_t>(
                std::max(font.glyph_height, 0));
            continue;
        }

        const auto base_x = static_cast<std::int64_t>(glyph.x) - font.origin_x;
        const auto base_y = static_cast<std::int64_t>(glyph.y) - font.origin_y;
        const auto first_x = std::max<std::int64_t>(
            0, static_cast<std::int64_t>(renderer.clip_left) - base_x);
        const auto last_x = std::min<std::int64_t>(
            font.glyph_width,
            static_cast<std::int64_t>(renderer.clip_right) - base_x);
        const auto first_y = std::max<std::int64_t>(
            0, static_cast<std::int64_t>(renderer.clip_top) - base_y);
        const auto last_y = std::min<std::int64_t>(
            font.glyph_height,
            static_cast<std::int64_t>(renderer.clip_bottom) - base_y);
        if (font.glyph_width <= 0 || font.glyph_height <= 0 ||
            first_x >= last_x || first_y >= last_y) {
            continue;
        }

        const auto& rows = font.encoded_rows[glyph_index];
        for (auto row = first_y; row < last_y; ++row) {
            if (row >= static_cast<std::int64_t>(rows.size())) {
                result.input_complete = false;
                ++result.glyph_rows_missing;
                continue;
            }
            const auto& encoded = rows[static_cast<std::size_t>(row)];
            std::size_t stream = 0;
            std::int64_t source_x = 0;
            while (source_x < font.glyph_width && stream < encoded.size()) {
                const auto run = encoded[stream++];
                const auto count = static_cast<std::int64_t>(run & 0x7FU);
                if (count == 0) {
                    result.input_complete = false;
                    break;
                }
                const auto run_end = std::min<std::int64_t>(
                    font.glyph_width, source_x + count);
                if ((run & 0x80U) != 0) {
                    for (auto column = std::max(source_x, first_x);
                         column < std::min(run_end, last_x); ++column) {
                        if (stream >= encoded.size()) {
                            result.input_complete = false;
                            break;
                        }
                        const auto color_index = encoded[stream++];
                        const auto palette_index = palette_base + color_index;
                        if (palette_index >= palette.size()) {
                            result.input_complete = false;
                            ++result.palette_indices_missing;
                            continue;
                        }
                        const auto pixel_x = base_x + column;
                        const auto pixel_y = base_y + row;
                        const auto offset = pixel_y *
                            renderer.framebuffer_pitch_bytes + pixel_x * 2;
                        result.pixel_writes.push_back({
                            glyph_call_index,
                            static_cast<std::int32_t>(pixel_x),
                            static_cast<std::int32_t>(pixel_y),
                            offset,
                            static_cast<std::uint32_t>(
                                static_cast<std::int64_t>(renderer.framebuffer_base) + offset),
                            palette[palette_index],
                            2,
                        });
                    }
                    // Palette bytes for clipped pixels still occupy the stream.
                    const auto skipped_pixels = std::max<std::int64_t>(
                        0, std::min(run_end, first_x) - source_x);
                    const auto skipped_after_clip = std::max<std::int64_t>(
                        0, run_end - std::max(first_x, last_x));
                    stream += static_cast<std::size_t>(
                        skipped_pixels + skipped_after_clip);
                }
                source_x = run_end;
            }
            if (source_x < font.glyph_width && stream >= encoded.size()) {
                result.input_complete = false;
            }
        }
    }
    return result;
}

SceneDashboardCoreCallbackPlan scene_dashboard_core_callback_plan(
    const SceneDashboardCoreCallbackObservation& observation)
{
    SceneDashboardCoreCallbackPlan result{};
    result.next_state = observation.state;
    result.position = update_scene_dashboard_position(
        result.next_state, observation.position);

    const SceneDashboardLapObservation lap_observation{
        observation.selected_object_type_0x18,
        observation.global_method_0x40_result,
        observation.record_value_0x138,
        observation.global_method_0x20_result,
    };
    result.lap = update_scene_dashboard_lap(
        result.next_state, lap_observation, result.position.redraw_all);

    const SceneDashboardFinalTimeObservation final_time_observation{
        observation.selected_object_type_0x18,
        observation.global_method_0x40_result,
    };
    result.final_time = update_scene_dashboard_final_time(
        result.next_state, final_time_observation, result.position.redraw_all);

    if (observation.selected_object_type_0x18 == 0 &&
        !observation.global_method_0x40_result) {
        auto lap_time_observation = observation.lap_time;
        lap_time_observation.redraw_all = result.position.redraw_all;
        lap_time_observation.redraw_from_prior_state_change =
            observation.state.dashboard_kind_cache_0x4cf9ec != 1;
        result.lap_time = update_scene_dashboard_lap_time(
            result.next_state, lap_time_observation);
    }

    if (observation.selected_object_type_0x18 != 0) {
        auto test_track_observation = observation.test_track;
        test_track_observation.redraw_all = result.position.redraw_all;
        test_track_observation.displayed_position =
            observation.record_value_0x138;
        test_track_observation.selected_record_type_0x18 =
            observation.selected_object_type_0x18;
        result.test_track_branch_reached = true;
        result.test_track = update_scene_dashboard_test_track(
            result.next_state, test_track_observation);
    }

    if (observation.selected_object_type_0x18 == 0 &&
        !observation.global_method_0x40_result &&
        result.position.displayed_count > 1) {
        auto supplemental_observation = observation.supplemental_time;
        supplemental_observation.displayed_position =
            result.position.displayed_position;
        supplemental_observation.displayed_count =
            result.position.displayed_count;
        result.supplemental_time_branch_reached = true;
        result.supplemental_time = scene_dashboard_supplemental_time_plan(
            supplemental_observation);
    }

    for (std::size_t i = 0; i < result.metrics.size(); ++i) {
        auto metric_observation = observation.metrics[i];
        metric_observation.slot = i == 0
            ? SceneDashboardMetricSlot::value_0x10
            : SceneDashboardMetricSlot::value_0x14;
        metric_observation.selected_object_type_0x18 =
            observation.selected_object_type_0x18;
        result.metrics[i] = update_scene_dashboard_metric(
            result.next_state, metric_observation, result.position.redraw_all);
    }

    auto auxiliary_observation = observation.auxiliary_readout;
    auxiliary_observation.redraw_all = result.position.redraw_all;
    result.auxiliary_readout_reached = true;
    result.auxiliary_readout = scene_dashboard_auxiliary_readout_plan(
        auxiliary_observation);

    result.reached_text_call_site_mask =
        result.position.reached_text_call_sites |
        result.lap.reached_text_call_sites |
        result.final_time.reached_text_call_sites;
    result.ordered_text_calls = scene_dashboard_text_plan(
        result.reached_text_call_site_mask);
    const auto append_calls = [&](const auto& plan) {
        result.ordered_text_calls.insert(result.ordered_text_calls.end(),
            plan.calls.begin(), plan.calls.end());
    };
    if (observation.selected_object_type_0x18 == 0 &&
        !observation.global_method_0x40_result) {
        append_calls(result.lap_time);
    }
    if (result.test_track_branch_reached) {
        append_calls(result.test_track);
    }
    if (result.supplemental_time_branch_reached) {
        append_calls(result.supplemental_time);
    }
    for (const auto& metric : result.metrics) {
        append_calls(metric);
    }
    append_calls(result.auxiliary_readout);
    return result;
}

SceneDashboardLapTimeResult update_scene_dashboard_lap_time(
    SceneDashboardState& state,
    const SceneDashboardLapTimeObservation& observation)
{
    SceneDashboardLapTimeResult result{};
    if ((state.resource_init_mask_0x4eb484 & 0x08U) == 0) {
        state.resource_init_mask_0x4eb484 |= 0x08U;
        result.resource_registered = true;
    }

    const auto value_0x0c = std::bit_cast<float>(
        observation.record_value_0x0c_bits);
    const auto value_0x14 = std::bit_cast<float>(
        observation.record_value_0x14_bits);
    const auto raw_0x0c = std::bit_cast<std::int32_t>(
        observation.record_value_0x0c_bits);
    const auto raw_0x14 = std::bit_cast<std::int32_t>(
        observation.record_value_0x14_bits);
    result.positive_delta_branch = raw_0x14 > 0x3F800000 &&
        raw_0x0c < 0x40A00000;

    if (result.positive_delta_branch) {
        state.dashboard_kind_cache_0x4cf9ec = 3;
        state.redraw_mask_0x4eb40c |= 0x80U;
        if (!observation.redraw_all) {
            result.requested_dirty_rectangles = 0x80U;
        }

        const auto converted = truncate_to_low_i32(
            static_cast<long double>(value_0x0c) * 8.0L);
        const auto color = (converted & 1) != 0
            ? 0x004EB3F4U
            : 0x004EB488U;
        result.formatted_time = format_scene_dashboard_time(
            value_0x14, observation.time_marker, true);
        result.first_line_text = observation.resource_text_0x4eb2b4;
        result.second_line_text = result.formatted_time;
        result.refresh_resource = !observation.resource_generation_matches;
        result.calls.push_back({0x00403A3F, 336, 35, 0x004CFCC0,
                                color, 0x004EB304, 0x22});
        result.calls.push_back({0x00403A66, 342, 35, 0x004CFCC4,
                                color, 0x004EB304, 0x21});
        return result;
    }

    result.fallback_label_drawn = observation.redraw_all ||
        observation.redraw_from_prior_state_change;
    result.first_line_text = observation.resource_text_0x4eb2b4;
    result.formatted_time = format_scene_dashboard_time(
        value_0x0c, observation.time_marker, false);
    result.second_line_text = result.formatted_time;
    if (result.fallback_label_drawn) {
        result.refresh_resource = !observation.resource_generation_matches;
        result.calls.push_back({0x00403AB1, 336, 35, 0x004CFCC8,
                                0x004EB488, 0x004EB304, 0x21});
    }
    result.calls.push_back({0x00403AD8, 342, 35, 0x004CFCCC,
                            0x004EB488, 0x004EB304, 0x21});
    return result;
}

SceneDashboardTestTrackResult update_scene_dashboard_test_track(
    SceneDashboardState& state,
    const SceneDashboardTestTrackObservation& observation)
{
    SceneDashboardTestTrackResult result{};
    result.dashboard_kind_changed =
        state.dashboard_kind_cache_0x4cf9ec != 2;
    if (result.dashboard_kind_changed) {
        state.dashboard_kind_cache_0x4cf9ec = 2;
        state.redraw_mask_0x4eb40c |= 0x80U;
        if (!observation.redraw_all) {
            result.requested_dirty_rectangles |= 0x80U;
        }
    }

    constexpr std::uint32_t test_track_resource_bits = 0x70U;
    result.newly_registered_resource_bits =
        test_track_resource_bits & ~state.resource_init_mask_0x4eb484;
    state.resource_init_mask_0x4eb484 |= test_track_resource_bits;

    result.try_label_drawn = observation.redraw_all ||
        result.dashboard_kind_changed ||
        observation.redraw_from_prior_state_change;
    result.try_label_text = observation.try_resource_text;
    if (result.try_label_drawn) {
        result.refresh_try_resource =
            !observation.try_resource_generation_matches;
        result.calls.push_back({0x00403DA8, 266, 18, 0x004CFD4C,
                                0x004EB314, 0x004EB2A8, 0x21});
    }

    result.position_row_drawn = observation.redraw_all ||
        state.lap_value_cache_0x4cf9d0 != observation.displayed_position;
    if (result.position_row_drawn) {
        state.lap_value_cache_0x4cf9d0 = observation.displayed_position;
        if (!observation.redraw_all) {
            result.requested_dirty_rectangles |= 0x04U;
        }
        state.redraw_mask_0x4eb40c |= 0x04U;
        result.calls.push_back({0x00403DF1, 306, 18, 0x004CFD50,
                                0x004EB314, 0x004EB2A8, 0x21});
    }

    const bool resource_label_gate = observation.redraw_all ||
        result.dashboard_kind_changed ||
        observation.redraw_from_prior_state_change;
    const auto record_time = std::bit_cast<float>(
        observation.record_value_0x0c_bits);
    if (observation.selected_record_type_0x18 != 5) {
        result.time_resource_label_text = observation.time_resource_text;
        result.time_resource_label_drawn = resource_label_gate;
        if (result.time_resource_label_drawn) {
            result.refresh_time_resource =
                !observation.time_resource_generation_matches;
            result.calls.push_back({0x00403E53, 242, 35, 0x004CFD54,
                                    0x004EB488, 0x004EB304, 0x21});
        }

        result.time_value_text = format_scene_dashboard_time(
            record_time, observation.time_marker, false);
        const auto time_format =
            observation.selected_record_type_0x18 == 3
                ? 0x004CFD5CU
                : 0x004CFD58U;
        const auto time_call =
            observation.selected_record_type_0x18 == 3
                ? 0x00403EC1U
                : 0x00403E90U;
        result.calls.push_back({time_call, 342, 35, time_format,
                                0x004EB488, 0x004EB304, 0x21});
    } else {
        result.distance_resource_label_text =
            observation.distance_resource_text;
        result.distance_resource_label_drawn = resource_label_gate;
        if (result.distance_resource_label_drawn) {
            result.refresh_distance_resource =
                !observation.distance_resource_generation_matches;
            result.calls.push_back({0x00403F12, 242, 35, 0x004CFD60,
                                    0x004EB488, 0x004EB304, 0x21});
        }

        const auto distance_scale = std::bit_cast<float>(
            observation.distance_scale_0x0c_bits);
        const auto distance = static_cast<long double>(record_time) *
            static_cast<long double>(distance_scale);
        const auto numeric_text = replace_decimal_marker(
            format_printf_string("%1.0f", static_cast<double>(distance)),
            observation.time_marker);
        result.distance_value_text = format_printf_string(
            "%s %s", numeric_text.c_str(),
            observation.distance_suffix_0x08.c_str());
        result.calls.push_back({0x00403F76, 342, 35, 0x004CFD6C,
                                0x004EB488, 0x004EB304, 0x21});
    }
    return result;
}

std::optional<std::uint8_t> scene_chatline_virtual_row_flag(
    const std::span<const std::byte> object_bytes,
    const std::uint32_t index) noexcept
{
    constexpr std::size_t base_offset = 0x1C8U;
    constexpr std::size_t row_stride = 0xD9U;
    if (index > (std::numeric_limits<std::size_t>::max() - base_offset) /
            row_stride) {
        return std::nullopt;
    }
    const auto offset = base_offset + static_cast<std::size_t>(index) * row_stride;
    if (offset >= object_bytes.size()) {
        return std::nullopt;
    }
    return std::to_integer<std::uint8_t>(object_bytes[offset]);
}

bool scene_dashboard_string_table_mode_2_predicate(
    const SceneDashboardMode2PredicateObservation& observation) noexcept
{
    const auto object_state = observation.chatline_lookup_result_0x49fa00
        ? (*observation.chatline_lookup_result_0x49fa00 == -1 ? 3 : 6)
        : observation.raw_vtable_field_0x04;
    if (!observation.global_interface_0x4e322c_present || object_state <= 4) {
        return false;
    }
    auto method_result = observation.virtual_method_0x84_return_low_byte;
    if (!observation.chatline_object_bytes.empty()) {
        if (!observation.virtual_method_index) return false;
        const auto row_flag = scene_chatline_virtual_row_flag(
            observation.chatline_object_bytes, *observation.virtual_method_index);
        if (!row_flag) return false;
        method_result = *row_flag;
    }
    return method_result != 0;
}

std::optional<SceneDashboardStringTableSetObservation>
make_scene_dashboard_string_table_set_observation(
    const SceneDashboardStringTableConstructorObservation& observation)
{
    constexpr std::size_t runtime_index_offset = 0x194U;
    constexpr std::size_t runtime_index_end = runtime_index_offset + sizeof(std::uint32_t);
    constexpr std::size_t configured_record_size = 0xc8U;

    if (observation.runtime_car_record.size() < runtime_index_end ||
        observation.configured_car_record.size() < configured_record_size) {
        return std::nullopt;
    }

    const auto read_u32 = [](const std::span<const std::byte> bytes,
                             const std::size_t offset) {
        return static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset])) |
            (static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 1U])) << 8U) |
            (static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 2U])) << 16U) |
            (static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 3U])) << 24U);
    };
    const auto read_c_string = [](const std::span<const std::byte> bytes,
                                 const std::size_t offset)
        -> std::optional<std::string> {
        if (offset >= bytes.size()) {
            return std::nullopt;
        }
        std::string value;
        for (std::size_t index = offset; index < bytes.size(); ++index) {
            const auto character = std::to_integer<unsigned char>(bytes[index]);
            if (character == 0U) {
                return value;
            }
            value.push_back(static_cast<char>(character));
        }
        return std::nullopt;
    };

    const auto text_0x05 = read_c_string(observation.configured_car_record, 0x04U);
    const auto text_0x12 = read_c_string(observation.configured_car_record, 0x11U);
    if (!text_0x05.has_value() || !text_0x12.has_value()) {
        return std::nullopt;
    }

    SceneDashboardStringTableSetObservation result{};
    result.index = std::bit_cast<std::int32_t>(
        read_u32(observation.runtime_car_record, runtime_index_offset));
    result.raw_mode_0x00 = read_u32(observation.configured_car_record, 0U);
    result.raw_value_0x134 = observation.object_field_0x3c_address;
    result.mode_2_predicate = observation.mode_2_predicate;
    result.text_0x05 = *text_0x05;
    result.text_0x12 = *text_0x12;
    return result;
}

void initialize_scene_dashboard_string_table(
    SceneDashboardStringTable& table) noexcept
{
    for (auto& entry : table.entries) {
        entry = {};
    }
    table.entry_count = 0;
}

bool set_scene_dashboard_string_table_entry(
    SceneDashboardStringTable& table,
    const SceneDashboardStringTableSetObservation& observation)
{
    if (observation.index < 0 || observation.index >= 16) {
        return false;
    }

    auto& entry = table.entries[static_cast<std::size_t>(observation.index)];
    entry.initialized = true;
    entry.raw_mode_0x00 = observation.raw_mode_0x00;
    entry.enabled_0x04 = observation.raw_mode_0x00 == 0 ||
        (observation.raw_mode_0x00 == 2 &&
         scene_dashboard_string_table_mode_2_predicate(
             observation.mode_2_predicate));
    const auto native_c_string = [](const std::string& value) {
        const std::string_view view{value};
        return view.substr(0, view.find('\0'));
    };
    const auto text_0x05 = native_c_string(observation.text_0x05);
    const auto text_0x12 = native_c_string(observation.text_0x12);
    entry.text_0x05.assign(text_0x05);
    entry.text_0x12.assign(text_0x12);
    entry.raw_value_0x134 = observation.raw_value_0x134;
    entry.text_0x05_copy_bytes = text_0x05.size() + 1U;
    entry.text_0x12_copy_bytes = text_0x12.size() + 1U;
    entry.text_0x05_overwrites_text_0x12 =
        entry.text_0x05_copy_bytes > 0x12U - 0x05U;
    entry.text_0x05_overwrites_raw_value =
        entry.text_0x05_copy_bytes > 0x134U - 0x05U;
    entry.text_0x05_passes_entry_end =
        entry.text_0x05_copy_bytes > 0x170U - 0x05U;
    entry.text_0x12_overwrites_raw_value =
        entry.text_0x12_copy_bytes > 0x134U - 0x12U;
    entry.text_0x12_passes_entry_end =
        entry.text_0x12_copy_bytes > 0x170U - 0x12U;
    table.entry_count = std::max(
        table.entry_count,
        static_cast<std::uint32_t>(observation.index) + 1U);
    return true;
}

std::optional<std::string_view> scene_dashboard_string_table_text_0x05(
    const SceneDashboardStringTable& table,
    const std::int32_t index) noexcept
{
    if (index < 0 || static_cast<std::uint32_t>(index) >= table.entry_count ||
        index >= static_cast<std::int32_t>(table.entries.size())) {
        return std::nullopt;
    }

    const auto& entry = table.entries[static_cast<std::size_t>(index)];
    if (!entry.initialized) {
        return std::nullopt;
    }
    return std::string_view{entry.text_0x05};
}

SceneDashboardSupplementalTimeResult scene_dashboard_supplemental_time_plan(
    const SceneDashboardSupplementalTimeObservation& observation)
{
    constexpr auto threshold_tenth = std::int32_t{0x3DCCCCCD};
    constexpr auto threshold_one = std::int32_t{0x3F800000};
    constexpr auto threshold_thirty = std::int32_t{0x41F00000};
    const auto value_0x1c = std::bit_cast<std::int32_t>(
        observation.record_value_0x1c_bits);
    const auto value_0x24 = std::bit_cast<std::int32_t>(
        observation.record_value_0x24_bits);

    SceneDashboardSupplementalTimeResult result{};
    if (observation.displayed_count <= 1) {
        return result;
    }

    result.show_0x24_branch = observation.displayed_position != 1 &&
        value_0x24 > threshold_tenth;
    result.show_0x1c_branch =
        observation.displayed_position != observation.displayed_count &&
        value_0x1c > threshold_tenth;

    const auto table_text_or = [&](const std::int32_t index,
                                   const std::string& fallback) {
        if (observation.string_table != nullptr) {
            const auto text = scene_dashboard_string_table_text_0x05(
                *observation.string_table, index);
            if (text.has_value()) {
                return std::string(*text);
            }
        }
        return fallback;
    };

    const auto formatted_value = [&](const std::uint32_t bits,
                                     const char* const format) {
        const auto value = std::bit_cast<float>(bits);
        // The x87 sequence selects max(value, 30.0f); unordered values retain
        // their NaN payload for the subsequent formatter.
        const auto adjusted = value < 30.0F ? 30.0F : value;
        return replace_decimal_marker(
            format_printf_string(format, static_cast<double>(adjusted)),
            observation.decimal_marker);
    };
    if (result.show_0x24_branch) {
        result.formatted_negative_value_0x24 = formatted_value(
            observation.record_value_0x24_bits, "-%03.1f");
        result.record_text_0x28_plus_5 = table_text_or(
            observation.record_index_0x28,
            observation.record_text_0x28_plus_5);
    }
    if (result.show_0x1c_branch) {
        result.formatted_positive_value_0x1c = formatted_value(
            observation.record_value_0x1c_bits, "+%03.1f");
        result.record_text_0x20_plus_5 = table_text_or(
            observation.record_index_0x20,
            observation.record_text_0x20_plus_5);
    }

    const auto color_for = [&](const std::int32_t value) {
        auto color = 0x004EB488U;
        if (value > threshold_one) {
            color = 0x004EB314;
        }
        if (value > threshold_thirty) {
            color = 0x004EB3F4;
        }
        return color;
    };
    const auto append_branch = [&](const bool reached,
                                   const std::uint32_t first_call_site,
                                   const std::uint32_t second_call_site,
                                   const std::uint32_t first_format,
                                   const std::uint32_t second_format,
                                   const std::uint32_t value,
                                   const std::int32_t y) {
        if (!reached) {
            return;
        }
        const auto color = color_for(std::bit_cast<std::int32_t>(value));
        result.calls.push_back({first_call_site, 83, y, first_format,
                                color, 0x004EB2A8, 0x0A});
        result.calls.push_back({second_call_site, 87, y, second_format,
                                color, 0x004EB2A8, 0x09});
    };

    append_branch(result.show_0x24_branch, 0x00403BAC, 0x00403BD7,
                  0x004CFCD8, 0x004CFCDC,
                  observation.record_value_0x24_bits, 25);
    append_branch(result.show_0x1c_branch, 0x00403C63, 0x00403C8E,
                  0x004CFCE8, 0x004CFCEC,
                  observation.record_value_0x1c_bits, 37);
    return result;
}

SceneSharedOverlayGates scene_shared_overlay_gates(
    const SceneSharedOverlayObservation& observation) noexcept
{
    const bool results_or_splash = observation.results_flag_0x4eb2f8 ||
        observation.splash_loading_flag_0x4eb1e8;
    return {
        results_or_splash,
        !results_or_splash && observation.paused_flag_0x508760 &&
            !observation.pause_suppression_flag_0x4ecb40,
        !results_or_splash &&
            (observation.cheater_flag_0x4eb2fc != 0 ||
             observation.cheater_flag_0x4eb2fd != 0) &&
            observation.world_index_0x53a4b8 < 12,
        !results_or_splash && observation.state_flag_0x4eb2fa,
        !results_or_splash && observation.state_flag_0x4eb2f9,
        !results_or_splash && observation.state_flag_0x4eb2fb,
        observation.results_flag_0x4eb2f8,
        observation.splash_loading_flag_0x4eb1e8,
    };
}

SceneSharedCheaterBannerCall scene_shared_cheater_banner_call(
    const bool gate_reached,
    const std::int32_t surface_width,
    const std::int32_t surface_height,
    const std::int32_t text_width,
    const std::int32_t text_height,
    const std::uint32_t world_index) noexcept
{
    SceneSharedCheaterBannerCall result{};
    if (!gate_reached) {
        return result;
    }

    const auto difference = 5 - static_cast<std::int32_t>(world_index);
    auto draw_mode = difference;
    if (difference < 0) {
        // This preserves the decompiler-visible low-bit reduction for the
        // negative case; the resulting renderer mode has no known semantics.
        draw_mode = (-difference) & 1;
    }
    result.reached = true;
    result.x = surface_width / 2 - text_width / 2;
    result.y = (surface_height / 3 - 0x14) - text_height / 2;
    result.raw_draw_mode = draw_mode;
    return result;
}

SceneSharedCheaterBannerPlan scene_shared_cheater_banner_plan(
    const SceneSharedCheaterBannerObservation& observation) noexcept
{
    SceneSharedCheaterBannerPlan result{};
    result.gates = scene_shared_overlay_gates(observation.overlay_globals);
    result.draw = scene_shared_cheater_banner_call(
        result.gates.cheater_banner,
        observation.surface_width, observation.surface_height,
        observation.text_width, observation.text_height,
        observation.overlay_globals.world_index_0x53a4b8);
    return result;
}

SceneSharedCenteredResourceCall scene_shared_centered_resource_call(
    const bool gate_reached,
    const std::int32_t surface_width,
    const std::int32_t surface_height,
    const std::int32_t resource_width) noexcept
{
    SceneSharedCenteredResourceCall result{};
    result.reached = gate_reached;
    if (!gate_reached) {
        return result;
    }
    result.x = (surface_width - resource_width) / 2;
    result.y = surface_height / 2 - 0x82;
    return result;
}

SceneSharedCenteredResourcePlan scene_shared_centered_resource_plan(
    const SceneSharedCenteredResourceObservation& observation) noexcept
{
    SceneSharedCenteredResourcePlan result{};
    result.gates = scene_shared_overlay_gates(observation.overlay_globals);
    result.draw = scene_shared_centered_resource_call(
        result.gates.state_flag_0x4eb2fb,
        observation.surface_width, observation.surface_height,
        observation.resource_width);
    return result;
}

SceneSharedPausedBannerCall scene_shared_paused_banner_call(
    const bool gate_reached,
    const bool resource_generation_matches) noexcept
{
    SceneSharedPausedBannerCall result{};
    result.reached = gate_reached;
    result.initialize_resource = gate_reached &&
        !resource_generation_matches;
    return result;
}

ScenePausedBannerRenderPlan scene_paused_banner_render_plan(
    const ScenePausedBannerRenderObservation& observation)
{
    ScenePausedBannerRenderPlan result{};
    result.reached = observation.gate_reached;
    result.text_source = observation.text_handle;
    result.outer_box_color = observation.outer_box_color_0x4ecbc8;
    result.inner_box_color = observation.inner_box_color_0x4ecb90;
    if (!result.reached) {
        return result;
    }

    auto load_stp_found = observation.load_stp_resource_found;
    if (observation.load_stp_resource_lookup.has_value()) {
        auto lookup = *observation.load_stp_resource_lookup;
        lookup.normalized_name_first_byte = 'l';
        lookup.requested_type_code =
            resource_type_code('S', 'T', 'M', 'P');
        result.resource_lookup = plan_resource_get(lookup);
        result.input_complete = result.resource_lookup->input_complete;
        load_stp_found = result.resource_lookup->exit ==
            ResourceGetExit::returned_resource;
    }
    result.ordered_operations.push_back({
        ScenePausedBannerOperationKind::resource_lookup_0x44cfd0,
        result.resource_load_helper, {}, 0, 0, 0, 0,
        result.resource_name_source, 0});

    const auto center_x = observation.surface_width / 2;
    const auto center_y = observation.surface_height / 2;
    result.draws_fallback_box = !load_stp_found;
    result.draws_resource_text = load_stp_found;
    result.draws_text_handle = !load_stp_found ||
        observation.text_handle_nonnull;

    if (result.draws_fallback_box) {
        result.ordered_operations.push_back({
            ScenePausedBannerOperationKind::fallback_measure_width_0x479450,
            0x00479450U, {}, 0, 0,
            static_cast<std::int32_t>(result.raw_text_size_0xe), 0,
            result.text_source, result.raw_text_size_0xe});
        const auto half_width = (observation.fallback_text_width + 0x14) / 2;
        result.outer_box = {
            center_x - half_width,
            center_y - 0x0F,
            center_x + half_width,
            center_y + 0x0F,
        };
        result.inner_box = {
            result.outer_box[0] + 2,
            result.outer_box[1] + 2,
            result.outer_box[2] - 2,
            result.outer_box[3] - 2,
        };
        result.ordered_operations.push_back({
            ScenePausedBannerOperationKind::outer_box_fill_0x44a1b0,
            result.box_draw_helper, result.outer_box, 0, 0, 0,
            result.outer_box_color});
        result.ordered_operations.push_back({
            ScenePausedBannerOperationKind::inner_box_fill_0x44a1b0,
            result.box_draw_helper, result.inner_box, 0, 0, 0,
            result.inner_box_color});
    }
    if (result.draws_resource_text) {
        result.ordered_operations.push_back({
            ScenePausedBannerOperationKind::resource_measure_height_0x44d460,
            0x0044d460U, {}, 0, 0, 0, 0,
            result.resource_object, 0});
        result.ordered_operations.push_back({
            ScenePausedBannerOperationKind::resource_measure_width_0x44d450,
            0x0044d450U, {}, 0, 0, 0, 0,
            result.resource_object, 0});
        result.resource_x =
            (observation.surface_width - observation.resource_text_width) / 2;
        result.resource_y =
            (observation.surface_height - observation.resource_text_height) / 2;
        result.releases_resource = true;
        result.ordered_operations.push_back({
            ScenePausedBannerOperationKind::resource_draw_0x44d0c0,
            result.resource_draw_helper, {}, result.resource_x,
            result.resource_y, 0, 0, result.resource_object, 0});
    }
    result.text_x = center_x;
    result.text_y = center_y + 4;
    if (result.draws_text_handle) {
        result.ordered_operations.push_back({
            ScenePausedBannerOperationKind::text_draw_0x479580,
            result.text_draw_helper, {}, result.text_x, result.text_y,
            static_cast<std::int32_t>(result.raw_text_size_0xe), 0,
            result.text_source, result.raw_text_size_0xe});
    }
    if (result.releases_resource) {
        result.ordered_operations.push_back({
            ScenePausedBannerOperationKind::resource_release_0x44d0a0,
            result.resource_release_helper, {}, 0, 0, 0, 0,
            result.resource_object, 0});
    }
    return result;
}

std::vector<SceneRectangleFillPixelWrite> scene_paused_banner_box_pixel_writes(
    const ScenePausedBannerRenderPlan& plan,
    const RenderContextViewport& context,
    const std::uint8_t renderer_mode,
    const std::uint32_t framebuffer_base)
{
    std::vector<SceneRectangleFillPixelWrite> writes;
    for (const auto& operation : plan.ordered_operations) {
        if (operation.kind != ScenePausedBannerOperationKind::outer_box_fill_0x44a1b0 &&
            operation.kind != ScenePausedBannerOperationKind::inner_box_fill_0x44a1b0) {
            continue;
        }
        auto box_writes = scene_rectangle_fill_pixel_writes(
            operation.rectangle[0], operation.rectangle[1],
            operation.rectangle[2], operation.rectangle[3], operation.color,
            context, renderer_mode, framebuffer_base);
        writes.insert(writes.end(), box_writes.begin(), box_writes.end());
    }
    return writes;
}

SceneSharedPausedBannerPlan scene_shared_paused_banner_plan(
    const SceneSharedPausedBannerObservation& observation) noexcept
{
    SceneSharedPausedBannerPlan result{};
    result.gates = scene_shared_overlay_gates(observation.overlay_globals);
    result.outer_call = scene_shared_paused_banner_call(
        result.gates.paused_banner,
        observation.resource_generation_matches);
    auto render_observation = observation.render;
    render_observation.gate_reached = result.outer_call.reached;
    result.inner_render = scene_paused_banner_render_plan(render_observation);
    return result;
}

SceneSharedLoadingBannerPlan scene_shared_loading_banner_plan(
    const SceneSharedLoadingBannerObservation& observation)
{
    SceneSharedLoadingBannerPlan result{};
    result.gates = scene_shared_overlay_gates(observation.overlay_globals);
    result.reached = result.gates.splash_loading;
    result.next_resource_init_mask_0x4eb37c =
        observation.resource_init_mask_0x4eb37c;
    if (!result.reached) {
        return result;
    }

    if ((result.next_resource_init_mask_0x4eb37c & 2U) == 0U) {
        result.initialize_resource = true;
        result.next_resource_init_mask_0x4eb37c |= 2U;
        result.ordered_operations.push_back({
            SceneSharedLoadingBannerOperationKind::register_resource_0x418c90,
            result.register_helper, result.resource_object,
            result.cleanup_callback});
    }
    result.refresh_resource = !observation.resource_generation_matches;
    if (result.refresh_resource) {
        result.ordered_operations.push_back({
            SceneSharedLoadingBannerOperationKind::refresh_resource_0x418cc0,
            result.refresh_helper, result.resource_object, 0});
    }
    result.ordered_operations.push_back({
        SceneSharedLoadingBannerOperationKind::draw_resource_0x40c930,
        result.draw_helper, result.resource_handle, 0});
    return result;
}

SceneSharedTextEffectPlan scene_shared_text_effect_plan(
    const SceneSharedTextEffectObservation& observation,
    const bool resource_generation_matches)
{
    SceneSharedTextEffectPlan result{};
    result.reached = observation.gate_reached;
    result.initialize_resource = observation.gate_reached &&
        !resource_generation_matches;
    if (!result.reached) {
        return result;
    }

    if (result.initialize_resource) {
        result.calls.push_back({0x00418CC0, observation.resource_object, 0, 1});
    }
    result.calls.push_back({0x0044DB10, observation.font_object,
                            observation.text_handle, 2});
    result.calls.push_back({0x0044DC50, observation.font_object, 0, 1});
    result.has_text_draw = true;
    result.draw_call_site = observation.branch ==
            SceneSharedTextEffectBranch::predicate_0x4048e0
        ? 0x00404D6C
        : 0x00404DC1;
    result.draw_x = observation.branch ==
            SceneSharedTextEffectBranch::predicate_0x4048e0
        ? observation.surface_width - observation.text_width - 10
        : 10;
    result.draw_format_source = observation.text_handle;
    result.draw_font_source = observation.font_object;
    return result;
}

SceneSharedTextEffectsPlan scene_shared_text_effects_plan(
    const SceneSharedTextEffectsObservation& observation)
{
    SceneSharedTextEffectsPlan result{};
    result.gates = scene_shared_overlay_gates(observation.overlay_globals);

    auto right_text = observation.predicate_0x4048e0;
    right_text.branch = SceneSharedTextEffectBranch::predicate_0x4048e0;
    right_text.gate_reached = result.gates.wrong_way_banner;
    right_text.resource_object = 0x004EB428U;
    right_text.cached_resource_generation = 0x004EB430U;
    right_text.text_handle = 0x004EB42CU;
    result.predicate_0x4048e0 = scene_shared_text_effect_plan(
        right_text, observation.resource_0x4eb428_generation_matches);

    auto left_text = observation.predicate_0x404900;
    left_text.branch = SceneSharedTextEffectBranch::predicate_0x404900;
    left_text.gate_reached = result.gates.state_flag_0x4eb2f9;
    left_text.resource_object = 0x004EB290U;
    left_text.cached_resource_generation = 0x004EB298U;
    left_text.text_handle = 0x004EB294U;
    result.predicate_0x404900 = scene_shared_text_effect_plan(
        left_text, observation.resource_0x4eb290_generation_matches);
    return result;
}

SceneSharedResultsHeadingPlan scene_shared_results_heading_plan(
    const SceneSharedResultsHeadingObservation& observation)
{
    SceneSharedResultsHeadingPlan result{};
    result.next_resource_init_mask_0x4eb274 =
        observation.resource_init_mask_0x4eb274;
    result.next_prompt_init_mask_0x4eb37c =
        observation.prompt_init_mask_0x4eb37c;
    if (!observation.gate_reached) {
        return result;
    }

    result.reached = true;
    if ((result.next_resource_init_mask_0x4eb274 & 0x80U) == 0) {
        result.initialize_results_resource = true;
        result.next_resource_init_mask_0x4eb274 |= 0x80U;
        result.ordered_operations.push_back({
            SceneSharedResultsHeadingOperationKind::register_results_resource_0x418c90,
            result.resource_register_helper, result.results_resource_object,
            result.results_cleanup});
    }
    if ((result.next_prompt_init_mask_0x4eb37c & 1U) == 0) {
        result.initialize_prompt_resource = true;
        result.next_prompt_init_mask_0x4eb37c |= 1U;
        result.ordered_operations.push_back({
            SceneSharedResultsHeadingOperationKind::register_prompt_resource_0x418c90,
            result.resource_register_helper, result.prompt_resource_object,
            result.prompt_cleanup});
    }
    result.refresh_results_resource =
        !observation.results_resource_generation_matches;
    result.refresh_prompt_resource =
        !observation.prompt_resource_generation_matches;
    result.prompt_x =
        (observation.surface_width - observation.prompt_text_width) / 2;
    result.prompt_y =
        (observation.surface_height - observation.prompt_text_height) / 2;
    result.ordered_operations.push_back({
        SceneSharedResultsHeadingOperationKind::draw_prompt_resource_0x44d0c0,
        result.layout_helper, result.prompt_layout_object, 0,
        result.prompt_x, result.prompt_y, 0});
    if (result.refresh_results_resource) {
        result.ordered_operations.push_back({
            SceneSharedResultsHeadingOperationKind::refresh_results_resource_0x418cc0,
            result.initialize_helper, result.results_resource_object, 0});
    }
    result.ordered_operations.push_back({
        SceneSharedResultsHeadingOperationKind::draw_results_text_0x44dc70,
        result.draw_helper, result.results_resource_object, 0,
        observation.surface_width / 2,
        (observation.surface_height - 0xB4) / 2 - 8, 0x24, 0x004EB24CU,
        0x004EB374U, 0x004EB304U});
    if (result.refresh_prompt_resource) {
        result.ordered_operations.push_back({
            SceneSharedResultsHeadingOperationKind::refresh_prompt_resource_0x418cc0,
            result.initialize_helper, result.prompt_resource_object, 0});
    }
    result.ordered_operations.push_back({
        SceneSharedResultsHeadingOperationKind::draw_prompt_text_0x44dc70,
        result.draw_helper, result.prompt_resource_object, 0,
        observation.surface_width / 2,
        (observation.surface_height - 0xB4) / 2 + 0xBF, 0x24, 0x004EB414U,
        0x004EB374U, 0x004EB304U});
    return result;
}

std::vector<SceneSharedResultRowSelection> scene_shared_result_rows(
    const std::span<const SceneSharedResultsRecordObservation> records)
{
    std::vector<SceneSharedResultRowSelection> selected_rows;
    for (std::size_t ordinal_pass = 0;
         ordinal_pass < records.size() &&
             ordinal_pass < static_cast<std::size_t>(
                 std::numeric_limits<std::int32_t>::max());
         ++ordinal_pass) {
        const auto expected_ordinal =
            static_cast<std::int32_t>(ordinal_pass + 1);
        for (std::size_t record_index = 0; record_index < records.size();
             ++record_index) {
            const auto& record = records[record_index];
            if (record.type_at_0x00 == 3 ||
                record.ordinal_at_0x140 != expected_ordinal) {
                continue;
            }
            selected_rows.push_back({
                ordinal_pass,
                record_index,
                expected_ordinal,
                record.row_helper_global_0x4dc19c ==
                    static_cast<std::uint32_t>(record_index),
                record.text_at_0x05,
                record.virtual_method_0x40_result,
                static_cast<std::uint8_t>(record_index & 0xFFU),
            });
        }
    }
    return selected_rows;
}

std::vector<SceneSharedResultRowDisplay> scene_shared_result_row_displays(
    const std::span<const SceneSharedResultsRecordObservation> records,
    const std::span<const std::uint8_t> result_time_records,
    const std::size_t result_time_record_count,
    const char marker)
{
    const auto selected_rows = scene_shared_result_rows(records);
    std::vector<SceneSharedResultRowDisplay> displays;
    displays.reserve(selected_rows.size());
    for (const auto& selection : selected_rows) {
        SceneSharedResultRowDisplay display{};
        display.selection = selection;
        if (selection.emit_time_value) {
            display.time_seconds = scene_dashboard_time_for_ordinal(
                result_time_records, result_time_record_count,
                selection.time_lookup_ordinal);
            display.formatted_time = format_scene_dashboard_time(
                display.time_seconds, marker, true);
        }
        displays.push_back(std::move(display));
    }
    return displays;
}

std::vector<SceneSharedResultRowDraw> scene_shared_result_row_draws(
    const std::span<const SceneSharedResultsRecordObservation> records,
    const std::span<const SceneSharedResultRowDisplay> displays,
    const std::int32_t surface_height,
    const std::int32_t surface_width)
{
    const auto half_width = surface_width / 2;
    std::vector<SceneSharedResultRowDraw> draws;
    draws.reserve(displays.size() * 3U);
    std::size_t emitted_rows = 0;
    for (const auto& display : displays) {
        const auto& selection = display.selection;
        if (selection.record_index >= records.size()) {
            continue;
        }
        const auto row_y = (surface_height - 0xB4) / 2 + 0x11 +
            static_cast<std::int32_t>(emitted_rows) * 0x14;
        const auto color = selection.row_helper_result_matches_index
            ? 0x004EB374U
            : 0x004EB370U;
        draws.push_back({selection.ordinal_pass, selection.record_index,
                         0x00405004, half_width - 0x91, row_y, 0x004CFF38,
                         color, 0x004EB304, 0x21,
                         selection.displayed_ordinal, {}});
        draws.push_back({selection.ordinal_pass, selection.record_index,
                         0x00405025, half_width - 0x6B, row_y, 0x004CFF3C,
                         color, 0x004EB304, 0x21, 0, selection.text_at_0x05});
        if (selection.emit_time_value) {
            draws.push_back({selection.ordinal_pass, selection.record_index,
                             0x00405069, half_width + 0x41, row_y, 0x004CFF40,
                             color, 0x004EB304, 0x21, 0,
                             display.formatted_time});
        }
        ++emitted_rows;
    }
    return draws;
}

SceneSharedResultsBranchPlan scene_shared_results_branch_plan(
    const SceneSharedResultsBranchObservation& observation)
{
    SceneSharedResultsBranchPlan result{};
    result.gates = scene_shared_overlay_gates(observation.overlay_globals);
    auto heading_observation = observation.heading;
    heading_observation.gate_reached = result.gates.race_results;
    result.heading = scene_shared_results_heading_plan(heading_observation);
    if (!result.gates.race_results) {
        return result;
    }

    result.row_displays = scene_shared_result_row_displays(
        observation.records, observation.result_time_records,
        observation.result_time_record_count, observation.time_marker);
    result.row_draws = scene_shared_result_row_draws(
        observation.records, result.row_displays,
        observation.heading.surface_height, observation.heading.surface_width);
    return result;
}

SceneSharedResultsHeadingTextRasterPlan
raster_scene_shared_results_heading_text(
    const SceneSharedResultsHeadingPlan& heading,
    const SceneSharedResultsHeadingTextRasterObservation& observation,
    const SceneDashboardFontObservation& layout_font,
    const SceneDashboardAtlasFontObservation& atlas_font)
{
    SceneSharedResultsHeadingTextRasterPlan result{};
    if (!heading.reached) {
        return result;
    }

    std::size_t renderer_index = 0;
    for (const auto& operation : heading.ordered_operations) {
        std::uint32_t call_site = 0;
        std::string_view text;
        switch (operation.kind) {
        case SceneSharedResultsHeadingOperationKind::draw_results_text_0x44dc70:
            call_site = 0x00404F32U;
            if (operation.format_source != 0x004EB24CU) {
                result.input_complete = false;
            }
            text = observation.results_title_text;
            break;
        case SceneSharedResultsHeadingOperationKind::draw_prompt_text_0x44dc70:
            call_site = 0x00404F76U;
            if (operation.format_source != 0x004EB414U) {
                result.input_complete = false;
            }
            text = observation.prompt_text;
            break;
        default:
            continue;
        }

        if (renderer_index >= observation.per_call_renderers.size()) {
            result.input_complete = false;
            break;
        }

        const SceneDashboardTextCall call{
            call_site,
            operation.x,
            operation.y,
            operation.format_source,
            operation.color_source,
            operation.font_source,
            operation.flags,
        };
        const auto layout = layout_scene_dashboard_text(call, text, layout_font);
        const auto raster = raster_scene_dashboard_text(
            layout, atlas_font, observation.per_call_renderers[renderer_index]);
        ++renderer_index;
        ++result.heading_text_calls_rasterized;
        result.input_complete = result.input_complete && raster.input_complete;
        result.ordered_pixel_writes.insert(
            result.ordered_pixel_writes.end(), raster.pixel_writes.begin(),
            raster.pixel_writes.end());
    }

    if (renderer_index != observation.per_call_renderers.size()) {
        result.input_complete = false;
    }
    return result;
}

SceneSharedOverlayFramePlan scene_shared_overlay_frame_plan(
    const SceneSharedOverlayFrameObservation& observation)
{
    SceneSharedOverlayFramePlan result{};
    result.gates = scene_shared_overlay_gates(observation.overlay_globals);

    auto paused = observation.paused_banner;
    paused.overlay_globals = observation.overlay_globals;
    result.paused_banner = scene_shared_paused_banner_plan(paused);
    result.paused_banner_box_pixel_writes =
        scene_paused_banner_box_pixel_writes(
            result.paused_banner.inner_render,
            observation.render_context,
            observation.renderer_mode_0x4d7a48,
            observation.framebuffer_base_value);

    auto cheater = observation.cheater_banner;
    cheater.overlay_globals = observation.overlay_globals;
    result.cheater_banner = scene_shared_cheater_banner_plan(cheater);

    auto text_effects = observation.text_effects;
    text_effects.overlay_globals = observation.overlay_globals;
    result.text_effects = scene_shared_text_effects_plan(text_effects);

    auto centered_resource = observation.centered_resource;
    centered_resource.overlay_globals = observation.overlay_globals;
    result.centered_resource = scene_shared_centered_resource_plan(
        centered_resource);

    auto race_results = observation.race_results;
    race_results.overlay_globals = observation.overlay_globals;
    result.race_results = scene_shared_results_branch_plan(race_results);

    auto splash_loading = observation.splash_loading;
    splash_loading.overlay_globals = observation.overlay_globals;
    result.splash_loading = scene_shared_loading_banner_plan(splash_loading);
    return result;
}

SceneState5OverlayPlan state5_numeric_overlay_plan(
    const SceneState5OverlayObservation& observation)
{
    SceneState5OverlayPlan result{};
    result.quaternion = matrix3x3_to_quaternion(
        observation.matrix_0x507dc8,
        observation.fdiv_compatibility_enabled);
    result.fdiv_compatibility_branch =
        observation.fdiv_compatibility_enabled;
    result.background_call_arguments = {
        10, 10, 0x17C, 0x3C, observation.background_value_0x507d48,
    };
    if (observation.background_renderer_mode_0x4d7a48 == 5) {
        result.background_backend_helper = 0x0044A550;
    } else if (observation.background_renderer_mode_0x4d7a48 == 1) {
        result.background_backend_helper = 0x0044A640;
    }

    const auto angle = std::acos(static_cast<long double>(
        result.quaternion.quaternion[3]));
    const auto sine = std::sin(angle);
    const auto stored_angle = static_cast<float>(angle);
    const auto stored_sine = static_cast<float>(sine);
    const auto absolute_sine = std::abs(stored_sine);
    constexpr auto sine_threshold = std::bit_cast<float>(0x34000000U);

    if (!std::isfinite(absolute_sine) || absolute_sine < sine_threshold) {
        result.zero_rotation_vector = true;
    } else {
        // The game uses x87 FDIV or the Pentium compatibility helper here.
        // Host long-double division approximates both paths but does not
        // reproduce the Pentium correction itself.
        const auto scale = (static_cast<long double>(stored_angle) * 2.0L) /
            static_cast<long double>(absolute_sine);
        constexpr auto radians_to_degrees = 57.295780181884765625L;
        for (std::size_t axis = 0; axis < 3; ++axis) {
            const auto component = static_cast<float>(
                static_cast<long double>(result.quaternion.quaternion[axis]) *
                scale);
            result.rotation_vector_radians[axis] = component;
            result.rotation_vector_degrees[axis] =
                static_cast<long double>(component) * radians_to_degrees;
        }
    }

    result.text_calls[0] = {
        15,
        15,
        format_printf_string("TV Camera: %d/%d",
            observation.camera_index_0x4d488c,
            observation.camera_count_0x5081dc),
        observation.first_text_color_0x5081e0,
        0x005081E0,
        0x004D4B04,
    };
    result.text_calls[1] = {
        15,
        30,
        format_printf_string("(%5.1f, %5.1f, %5.1f)  %3.0f, %3.0f, %3.0f ",
            static_cast<double>(observation.position_0x507dec_df0_df4[0]),
            static_cast<double>(observation.position_0x507dec_df0_df4[1]),
            static_cast<double>(observation.position_0x507dec_df0_df4[2]),
            static_cast<double>(result.rotation_vector_degrees[0]),
            static_cast<double>(result.rotation_vector_degrees[1]),
            static_cast<double>(result.rotation_vector_degrees[2])),
        observation.second_text_color_0x507df8,
        0x00507DF8,
        0x004D4B18,
    };
    for (std::size_t call_index = 0;
         call_index < result.text_calls.size(); ++call_index) {
        const auto& text_call = result.text_calls[call_index];
        auto glyph_x = text_call.x;
        for (const auto raw_character : text_call.text) {
            const auto character = static_cast<std::uint8_t>(raw_character);
            if (character == 0) {
                break;
            }
            const auto printable = character >= 0x20U && character <= 0x7FU;
            const auto glyph_character = printable
                ? character
                : static_cast<std::uint8_t>('#');
            const auto glyph_call_index = result.glyph_calls.size();
            result.glyph_calls.push_back({
                call_index,
                glyph_x,
                text_call.y,
                glyph_character,
                text_call.color,
            });
            const auto& bitmap = detail::scene_font_8x8[
                static_cast<std::size_t>(glyph_character - 0x20U)];
            for (std::size_t row = 0; row < bitmap.size(); ++row) {
                for (std::size_t column = 0; column < 8; ++column) {
                    if ((bitmap[row] & (std::uint8_t{1} << column)) != 0) {
                        const auto pixel_x =
                            glyph_x + static_cast<std::int32_t>(column);
                        const auto pixel_y =
                            text_call.y + static_cast<std::int32_t>(row);
                        result.pixel_calls.push_back({glyph_call_index,
                            pixel_x, pixel_y, text_call.color});
                        if (pixel_x < observation.clip_left ||
                            pixel_y < observation.clip_top ||
                            pixel_x >= observation.clip_right ||
                            pixel_y >= observation.clip_bottom) {
                            continue;
                        }

                        const bool rgba32 =
                            observation.background_renderer_mode_0x4d7a48 == 5;
                        const auto bytes_per_pixel = rgba32 ? 4 : 2;
                        const auto offset =
                            static_cast<std::int64_t>(
                                observation.framebuffer_pitch_bytes) * pixel_y +
                            static_cast<std::int64_t>(pixel_x) * bytes_per_pixel;
                        auto stored_value = text_call.color;
                        if (rgba32) {
                            stored_value = 0xFF000000U |
                                (((text_call.color >> 19U) & 0xFCU) << 8U) |
                                (((text_call.color >> 24U) & 0xF8U) << 16U) |
                                ((text_call.color >> 13U) & 0xF8U);
                        } else if (observation.background_renderer_mode_0x4d7a48 == 3) {
                            stored_value &= 0xFFFFU;
                        } else {
                            stored_value >>= 16U;
                        }
                        result.pixel_writes.push_back({
                            glyph_call_index,
                            pixel_x,
                            pixel_y,
                            offset,
                            observation.framebuffer_base_value +
                                static_cast<std::uint32_t>(offset),
                            stored_value,
                            static_cast<std::uint8_t>(bytes_per_pixel),
                        });
                    }
                }
            }
            glyph_x += 8;
        }
    }
    return result;
}

ScenePausedBannerResourceRasterPlan raster_scene_paused_banner_resource(
    const ScenePausedBannerRenderPlan& banner,
    const ScenePausedBannerResourceRasterObservation& observation)
{
    ScenePausedBannerResourceRasterPlan result{};
    const auto draw = std::find_if(
        banner.ordered_operations.begin(), banner.ordered_operations.end(),
        [](const ScenePausedBannerOperation& operation) {
            return operation.kind ==
                ScenePausedBannerOperationKind::resource_draw_0x44d0c0;
        });
    if (draw == banner.ordered_operations.end()) {
        return result;
    }
    result.draw_reached = true;
    if (!observation.resource_pointer_nonnull ||
        !observation.image.pointer_table_complete ||
        !observation.image.rows_complete || observation.image.width <= 0 ||
        observation.image.height <= 0 || observation.image.frame_count <= 0 ||
        draw->raw_mode < 0 ||
        draw->raw_mode > std::numeric_limits<std::int16_t>::max() ||
        static_cast<std::size_t>(draw->raw_mode) >=
            static_cast<std::size_t>(observation.image.frame_count)) {
        result.input_complete = false;
        return result;
    }

    result.frame_index = static_cast<std::size_t>(draw->raw_mode);
    SceneDashboardAtlasFontObservation image_font{};
    image_font.glyph_width = observation.image.width;
    image_font.glyph_height = observation.image.height;
    image_font.origin_x = observation.image.origin_x;
    image_font.origin_y = observation.image.origin_y;
    image_font.glyph_count =
        static_cast<std::size_t>(observation.image.frame_count);
    image_font.encoded_rows = observation.image.encoded_rows;
    image_font.embedded_palette.reserve(512U);
    for (const auto& palette_plane : observation.image.palette_planes) {
        image_font.embedded_palette.insert(
            image_font.embedded_palette.end(),
            palette_plane.begin(), palette_plane.end());
    }
    SceneDashboardTextLayout image_layout{};
    image_layout.glyph_calls.push_back({
        draw->helper,
        draw->x,
        draw->y,
        0,
        static_cast<std::int16_t>(draw->raw_mode),
    });
    // 0x40C790 passes a null fifth argument to 0x44D0C0, selecting the
    // appropriate palette plane embedded in the resource object.
    auto native_renderer = observation.renderer;
    native_renderer.explicit_palette.clear();
    result.raster = raster_scene_dashboard_text(
        image_layout, image_font, native_renderer);
    result.input_complete = result.raster.input_complete;
    return result;
}

ScenePausedBannerStyle14Plan scene_paused_banner_style14_plan(
    const ScenePausedBannerStyle14Observation& observation) noexcept
{
    ScenePausedBannerStyle14Plan result{};
    result.descriptor_words = {{
        observation.font_name_source_0x55fe48,
        0U,
        0x0aU,
        0U,
        0x004deb0cU,
        0x24U,
        static_cast<std::uint32_t>(observation.global_x_offset_0x560000),
        static_cast<std::uint32_t>(observation.global_x_offset_0x560000),
        observation.font_name_source_0x55fe48,
        0U,
        0U,
        static_cast<std::uint32_t>(observation.global_x_offset_0x560000),
        static_cast<std::uint32_t>(observation.global_x_offset_0x560000),
    }};
    result.text_style = {
        0x00479580U,
        0,
        0,
        0x004d9cd4U,
        observation.resolved_color_source_0x44d530,
        observation.resolved_font_source_0x44da00,
        result.descriptor_words[1],
    };
    result.x_offset = observation.global_x_offset_0x560000;
    result.y_offset = 0x24;
    return result;
}

ScenePausedBannerTextRasterPlan raster_scene_paused_banner_text(
    const ScenePausedBannerRenderPlan& banner,
    const ScenePausedBannerTextRasterObservation& observation,
    const SceneDashboardFontObservation& layout_font,
    const SceneDashboardAtlasFontObservation& atlas_font)
{
    ScenePausedBannerTextRasterPlan result{};
    const auto text_operation = std::find_if(
        banner.ordered_operations.begin(), banner.ordered_operations.end(),
        [](const ScenePausedBannerOperation& operation) {
            return operation.kind ==
                ScenePausedBannerOperationKind::text_draw_0x479580;
        });
    if (text_operation == banner.ordered_operations.end() ||
        !observation.text_pointer_nonnull) {
        return result;
    }

    const auto projected_style = observation.style14.has_value()
        ? std::optional<ScenePausedBannerStyle14Plan>{
            scene_paused_banner_style14_plan(*observation.style14)}
        : std::nullopt;
    const auto& text_style = projected_style.has_value()
        ? projected_style->text_style
        : observation.style;
    const auto style_x_offset = projected_style.has_value()
        ? projected_style->x_offset
        : observation.style_x_offset_0x20;
    const auto style_y_offset = projected_style.has_value()
        ? projected_style->y_offset
        : observation.style_y_offset_0x1c;

    auto text_end = observation.text.find('\0');
    if (text_end == std::string_view::npos) {
        text_end = observation.text.size();
    }
    const auto text = observation.text.substr(0, text_end);
    result.expected_line_count = 1U + static_cast<std::size_t>(
        std::count(text.begin(), text.end(), '\n'));
    result.input_complete =
        observation.per_line_renderers.size() == result.expected_line_count;

    const auto add_i32 = [](const std::int32_t left,
                            const std::int32_t right) noexcept {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) +
            static_cast<std::uint32_t>(right));
    };
    const auto sub_i32 = [](const std::int32_t left,
                            const std::int32_t right) noexcept {
        return std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(left) -
            static_cast<std::uint32_t>(right));
    };
    auto first_line_y = text_operation->y;
    const auto flags = text_style.format_flags;
    if ((flags & 0x10U) != 0U || (flags & 0x40U) != 0U) {
        const auto line_count = std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(result.expected_line_count));
        const auto line_delta = sub_i32(1, line_count);
        const auto total_delta = std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(line_delta) *
            static_cast<std::uint32_t>(observation.font_extent_0x44dc50));
        first_line_y = add_i32(first_line_y, total_delta / 2);
    }

    auto line_begin = std::size_t{0};
    for (std::size_t line_index = 0;
         line_index < result.expected_line_count; ++line_index) {
        const auto newline = text.find('\n', line_begin);
        const auto line_end = newline == std::string_view::npos
            ? text.size()
            : newline;
        const auto line = text.substr(line_begin, line_end - line_begin);
        if (line.size() > 0xffU) {
            result.input_complete = false;
        } else if (line_index < observation.per_line_renderers.size()) {
            const SceneDashboardTextCall call{
                0x00479580U,
                add_i32(text_operation->x,
                        style_x_offset),
                add_i32(
                    add_i32(first_line_y, std::bit_cast<std::int32_t>(
                        static_cast<std::uint32_t>(line_index) *
                        static_cast<std::uint32_t>(
                            observation.font_extent_0x44dc50))),
                    style_y_offset),
                0x004D9CD4U,
                text_style.color_source,
                text_style.font_source,
                flags,
            };
            const auto layout = layout_scene_dashboard_text(
                call, line, layout_font);
            const auto raster = raster_scene_dashboard_text(
                layout, atlas_font,
                observation.per_line_renderers[line_index]);
            ++result.text_calls_rasterized;
            result.input_complete = result.input_complete &&
                raster.input_complete;
            result.ordered_pixel_writes.insert(
                result.ordered_pixel_writes.end(),
                raster.pixel_writes.begin(), raster.pixel_writes.end());
        }
        if (newline == std::string_view::npos) {
            break;
        }
        line_begin = newline + 1U;
    }
    if (result.text_calls_rasterized != result.expected_line_count) {
        result.input_complete = false;
    }
    return result;
}

SceneSharedTextEffectsRasterPlan raster_scene_shared_text_effects(
    const SceneSharedTextEffectsPlan& plan,
    const SceneSharedTextEffectsRasterObservation& observation,
    const SceneDashboardFontObservation& layout_font,
    const SceneDashboardAtlasFontObservation& atlas_font)
{
    SceneSharedTextEffectsRasterPlan result{};
    const std::array<const SceneSharedTextEffectPlan*, 2> effects{{
        &plan.predicate_0x4048e0,
        &plan.predicate_0x404900,
    }};
    for (std::size_t index = 0; index < effects.size(); ++index) {
        const auto& effect = *effects[index];
        if (!effect.has_text_draw) {
            continue;
        }
        if (!observation.text[index].has_value()) {
            result.input_complete = false;
            continue;
        }
        const SceneDashboardTextCall call{
            effect.draw_call_site,
            effect.draw_x,
            effect.draw_y,
            effect.draw_format_source,
            effect.draw_color_source,
            effect.draw_font_source,
            effect.draw_flags,
        };
        const auto layout = layout_scene_dashboard_text(
            call, *observation.text[index], layout_font);
        const auto raster = raster_scene_dashboard_text(
            layout, atlas_font, observation.per_call_renderers[index]);
        ++result.text_calls_rasterized;
        result.input_complete = result.input_complete && raster.input_complete;
        result.ordered_pixel_writes.insert(
            result.ordered_pixel_writes.end(), raster.pixel_writes.begin(),
            raster.pixel_writes.end());
    }
    return result;
}

SceneSharedResultsRasterPlan raster_scene_shared_results_rows(
    const std::span<const SceneSharedResultRowDraw> row_draws,
    const std::span<const SceneDashboardRasterObservation> per_call_renderers,
    const SceneDashboardFontObservation& layout_font,
    const SceneDashboardAtlasFontObservation& atlas_font)
{
    SceneSharedResultsRasterPlan result;
    if (row_draws.size() != per_call_renderers.size()) {
        result.input_complete = false;
    }
    const auto call_count = std::min(row_draws.size(), per_call_renderers.size());
    for (std::size_t index = 0; index < call_count; ++index) {
        const auto& draw = row_draws[index];
        std::string integer_text;
        std::string_view formatted_text = draw.text_argument;
        if (draw.format_source == 0x004CFF38U) {
            // The first results-row call uses the observed `%d` format handle;
            // its variadic integer argument is the displayed ordinal.
            integer_text = std::to_string(draw.integer_argument);
            formatted_text = integer_text;
        }
        const SceneDashboardTextCall call{
            draw.call_site,
            draw.x,
            draw.y,
            draw.format_source,
            draw.color_source,
            draw.font_source,
            draw.flags,
        };
        const auto layout = layout_scene_dashboard_text(
            call, formatted_text, layout_font);
        const auto raster = raster_scene_dashboard_text(
            layout, atlas_font, per_call_renderers[index]);
        ++result.row_calls_rasterized;
        result.input_complete = result.input_complete && raster.input_complete;
        result.ordered_pixel_writes.insert(
            result.ordered_pixel_writes.end(), raster.pixel_writes.begin(),
            raster.pixel_writes.end());
    }
    return result;
}

SceneDashboardCoreCallbackRasterPlan raster_scene_dashboard_core_callback(
    const SceneDashboardCoreCallbackPlan& callback,
    const SceneDashboardCoreCallbackRasterObservation& observation,
    const SceneDashboardFontObservation& layout_font,
    const SceneDashboardAtlasFontObservation& atlas_font)
{
    SceneDashboardCoreCallbackRasterPlan result{};
    result.expected_call_count = callback.ordered_text_calls.size();
    result.input_complete =
        observation.formatted_text_arguments.size() == result.expected_call_count &&
        observation.per_call_renderers.size() == result.expected_call_count;

    const auto call_count = std::min({
        result.expected_call_count,
        observation.formatted_text_arguments.size(),
        observation.per_call_renderers.size(),
    });
    for (std::size_t index = 0; index < call_count; ++index) {
        const auto layout = layout_scene_dashboard_text(
            callback.ordered_text_calls[index],
            observation.formatted_text_arguments[index],
            layout_font);
        const auto raster = raster_scene_dashboard_text(
            layout, atlas_font, observation.per_call_renderers[index]);
        ++result.text_arguments_used;
        ++result.renderer_contexts_used;
        ++result.text_calls_rasterized;
        result.input_complete = result.input_complete && raster.input_complete;
        result.ordered_pixel_writes.insert(
            result.ordered_pixel_writes.end(), raster.pixel_writes.begin(),
            raster.pixel_writes.end());
    }
    return result;
}

ScenePhysicsWheelBarLabelRasterPlan raster_scene_physics_wheel_bar_labels(
    const std::array<ScenePhysicsWheelBarPlan, 4>& bars,
    const ScenePhysicsWheelBarLabelRasterObservation& observation,
    const SceneDashboardFontObservation& layout_font,
    const SceneDashboardAtlasFontObservation& atlas_font)
{
    ScenePhysicsWheelBarLabelRasterPlan result{};
    for (std::size_t index = 0; index < bars.size(); ++index) {
        const ScenePhysicsWheelBarOperation* label_operation = nullptr;
        for (const auto& operation : bars[index].ordered_operations) {
            if (operation.kind ==
                ScenePhysicsWheelBarOperationKind::formatted_text_0x479580) {
                if (label_operation != nullptr) {
                    result.input_complete = false;
                }
                label_operation = &operation;
            }
        }
        if (label_operation == nullptr ||
            !bars[index].formatted_label_complete) {
            result.input_complete = false;
            continue;
        }
        if (label_operation->argument != 0x004D4C54U ||
            label_operation->x2 != 0x11) {
            result.input_complete = false;
        }

        const SceneDashboardTextCall call{
            0x00479580U,
            label_operation->x1,
            label_operation->y1,
            label_operation->argument,
            observation.text_style.color_source,
            observation.text_style.font_source,
            observation.text_style.format_flags,
        };
        const auto layout = layout_scene_dashboard_text(
            call, label_operation->text_argument, layout_font);
        const auto raster = raster_scene_dashboard_text(
            layout, atlas_font, observation.per_bar_renderers[index]);
        ++result.labels_rasterized;
        result.input_complete = result.input_complete && raster.input_complete;
        result.ordered_pixel_writes.insert(
            result.ordered_pixel_writes.end(), raster.pixel_writes.begin(),
            raster.pixel_writes.end());
    }
    if (result.labels_rasterized != bars.size()) {
        result.input_complete = false;
    }
    return result;
}

SceneTextPixelApplyResult apply_scene_shared_results_raster(
    const SceneSharedResultsRasterPlan& plan,
    const std::span<std::uint8_t> framebuffer)
{
    if (!plan.input_complete) {
        return {};
    }
    return apply_scene_text_pixel_writes(plan.ordered_pixel_writes, framebuffer);
}

SceneTextPixelApplyResult apply_scene_text_pixel_writes(
    const std::span<const SceneTextPixelWrite> writes,
    const std::span<std::uint8_t> framebuffer)
{
    for (const auto& write : writes) {
        if (write.offset_from_framebuffer_base < 0 ||
            (write.bytes_per_pixel != 2U && write.bytes_per_pixel != 4U)) {
            return {};
        }
        const auto offset = static_cast<std::uint64_t>(
            write.offset_from_framebuffer_base);
        const auto width = static_cast<std::uint64_t>(write.bytes_per_pixel);
        if (offset > framebuffer.size() || width > framebuffer.size() - offset) {
            return {};
        }
    }

    for (const auto& write : writes) {
        const auto offset = static_cast<std::size_t>(
            write.offset_from_framebuffer_base);
        for (std::uint8_t byte = 0; byte < write.bytes_per_pixel; ++byte) {
            framebuffer[offset + byte] = static_cast<std::uint8_t>(
                write.value >> (static_cast<unsigned>(byte) * 8U));
        }
    }
    return {true, writes.size()};
}

SceneDashboardLocaleState initialize_scene_dashboard_locale(
    const SceneDashboardLocaleObservation& observation) noexcept
{
    constexpr std::array<std::uint32_t, 13> date_table{
        0x00000000U, 0x3F800000U, 0x00000000U, 0x3A83126FU,
        0x00000000U, 0x40666666U, 0x00000000U, 0x3F800000U,
        0x00000000U, 0x3F800000U, 0x00000000U, 0x3F800000U,
        0x00000001U,
    };
    constexpr std::array<std::uint32_t, 13> other_table{
        0x00000000U, 0x4051D608U, 0x00000000U, 0x3A23D70AU,
        0x00000000U, 0x40100000U, 0x00000000U, 0x3E872B02U,
        0x00000000U, 0x3D79F409U, 0x00000000U, 0x400CD068U,
        0x00000000U,
    };

    const bool use_date_table = observation.short_date_first_character == '0';
    return {
        use_date_table,
        observation.decimal_marker,
        observation.locale_0x16_bytes,
        use_date_table ? date_table : other_table,
    };
}

std::string_view scene_dashboard_crt_string_argument(
    const std::uint32_t pointer_bits,
    const std::string_view pointed_to_text) noexcept
{
    return pointer_bits == 0 ? std::string_view{"(null)"} : pointed_to_text;
}

void bind_scene_dashboard_locale(
    SceneDashboardLapTimeObservation& observation,
    const SceneDashboardLocaleState& locale) noexcept
{
    observation.time_marker = locale.decimal_marker;
}

void bind_scene_dashboard_locale(
    SceneDashboardTestTrackObservation& observation,
    const SceneDashboardLocaleState& locale)
{
    observation.distance_scale_0x0c_bits = locale.raw_table_dwords[1];
    observation.time_marker = locale.decimal_marker;
    observation.distance_suffix_0x08 = std::string(
        scene_dashboard_crt_string_argument(
            locale.raw_table_dwords[0], observation.distance_suffix_0x08));
}

void bind_scene_dashboard_locale(
    SceneDashboardSupplementalTimeObservation& observation,
    const SceneDashboardLocaleState& locale) noexcept
{
    observation.decimal_marker = locale.decimal_marker;
}

void bind_scene_dashboard_locale(
    SceneDashboardMetricObservation& observation,
    const SceneDashboardLocaleState& locale)
{
    observation.time_global_value_0x0c_bits = locale.raw_table_dwords[1];
    observation.type_3_global_value_0x1c_bits = locale.raw_table_dwords[5];
    observation.time_marker = locale.decimal_marker;
    observation.global_text_0x08 = std::string(
        scene_dashboard_crt_string_argument(
            locale.raw_table_dwords[0], observation.global_text_0x08));
    observation.type_3_global_text_0x18 = std::string(
        scene_dashboard_crt_string_argument(
            locale.raw_table_dwords[4], observation.type_3_global_text_0x18));
}

void bind_scene_dashboard_locale(
    SceneDashboardAuxiliaryReadoutObservation& observation,
    const SceneDashboardLocaleState& locale)
{
    observation.global_value_0x1c_bits = locale.raw_table_dwords[5];
    observation.global_text_0x18 = std::string(
        scene_dashboard_crt_string_argument(
            locale.raw_table_dwords[4], observation.global_text_0x18));
}

SceneDashboardMetricResult update_scene_dashboard_metric(
    SceneDashboardState& state,
    const SceneDashboardMetricObservation& observation,
    const bool redraw_all)
{
    auto& cache = observation.slot == SceneDashboardMetricSlot::value_0x10
        ? state.metric_cache_0x4cf9d8_bits
        : state.metric_cache_0x4cf9dc_bits;
    const auto value = std::bit_cast<float>(observation.value_bits);
    const auto cached_value = std::bit_cast<float>(cache);
    // FCOMP sets C3 for both equality and unordered comparisons. The caller
    // tests only that bit, so NaNs follow the binary's cache-hit path too.
    const bool cache_hit = std::isnan(value) || std::isnan(cached_value) ||
        value == cached_value;

    SceneDashboardMetricResult result{};
    if (cache_hit && !redraw_all) {
        return result;
    }

    result.cache_updated = true;
    cache = observation.value_bits;
    if (observation.slot == SceneDashboardMetricSlot::value_0x10) {
        constexpr std::uint32_t best_resource_bit = 0x80U;
        if ((state.resource_init_mask_0x4eb484 & best_resource_bit) == 0) {
            state.resource_init_mask_0x4eb484 |= best_resource_bit;
            result.resource_registered = true;
        }
    } else {
        constexpr std::uint32_t last_resource_bit = 0x01U;
        if ((state.resource_init_mask_0x4eb2bc & last_resource_bit) == 0) {
            state.resource_init_mask_0x4eb2bc |= last_resource_bit;
            result.resource_registered = true;
        }
    }
    const auto dirty_bit = observation.slot == SceneDashboardMetricSlot::value_0x10
        ? 0x10U
        : 0x100U;
    state.redraw_mask_0x4eb40c |= dirty_bit;
    if (!redraw_all) {
        result.requested_dirty_rectangles = dirty_bit;
    }

    const auto raw_value = std::bit_cast<std::int32_t>(observation.value_bits);
    result.value_above_raw_threshold = raw_value > 0x3DCCCCCD;
    if (!result.value_above_raw_threshold) {
        return result;
    }
    result.refresh_resource = observation.selected_object_type_0x18 != 3 &&
        !observation.resource_generation_matches;

    const auto numeric_value = static_cast<double>(value);
    if (observation.selected_object_type_0x18 == 3) {
        result.intermediate_format_source =
            observation.slot == SceneDashboardMetricSlot::value_0x10
                ? 0x004CFD98
                : 0x004CFDDC;
        const auto record_value = static_cast<double>(std::bit_cast<float>(
            observation.type_3_record_value_0x18_bits));
        const auto multiplier = static_cast<double>(std::bit_cast<float>(
            observation.type_3_global_value_0x1c_bits));
        const auto secondary = record_value * multiplier;
        result.formatted_value = replace_decimal_marker(
            format_printf_string("%1.1f @ %1.0f %s", numeric_value, secondary,
                                 observation.type_3_global_text_0x18.c_str()),
            observation.time_marker);
        result.displayed_text = result.formatted_value;
    } else if (observation.selected_object_type_0x18 == 5) {
        result.intermediate_format_source =
            observation.slot == SceneDashboardMetricSlot::value_0x10
                ? 0x004CFD84
                : 0x004CFDC8;
        const auto multiplier = static_cast<double>(std::bit_cast<float>(
            observation.time_global_value_0x0c_bits));
        result.formatted_value = replace_decimal_marker(
            format_printf_string("%1.0f", numeric_value * multiplier),
            observation.time_marker);
        result.displayed_text = format_printf_string(
            "%s %s %s",
            observation.prefix_text_from_0x4eb344_or_0x4eb334.c_str(),
            result.formatted_value.c_str(), observation.global_text_0x08.c_str());
    } else {
        result.formatted_value = format_scene_dashboard_time(
            value, observation.time_marker, true);
        result.displayed_text = format_printf_string(
            "%s %s",
            observation.prefix_text_from_0x4eb344_or_0x4eb334.c_str(),
            result.formatted_value.c_str());
    }

    std::uint32_t call_site{};
    std::uint32_t format_source{};
    std::int32_t x{};
    if (observation.slot == SceneDashboardMetricSlot::value_0x10) {
        x = 222;
        if (observation.selected_object_type_0x18 == 3) {
            call_site = 0x0040409D;
            format_source = 0x004CFDAC;
        } else if (observation.selected_object_type_0x18 == 5) {
            call_site = 0x00404122;
            format_source = 0x004CFD8C;
        } else {
            call_site = 0x00404030;
            format_source = 0x004CFDB0;
        }
    } else {
        x = 342;
        if (observation.selected_object_type_0x18 == 3) {
            call_site = 0x00404266;
            format_source = 0x004CFDF0;
        } else if (observation.selected_object_type_0x18 == 5) {
            call_site = 0x004042EB;
            format_source = 0x004CFDD0;
        } else {
            call_site = 0x004041F3;
            format_source = 0x004CFDF4;
        }
    }
    result.calls.push_back({call_site, x, 50, format_source,
                            0x004EB314, 0x004EB2A8, 0x21});
    return result;
}

SceneDashboardAuxiliaryReadoutResult scene_dashboard_auxiliary_readout_plan(
    const SceneDashboardAuxiliaryReadoutObservation& observation)
{
    const auto record_value = std::bit_cast<float>(
        observation.record_value_0x4c_bits);
    const auto global_multiplier = std::bit_cast<float>(
        observation.global_value_0x1c_bits);
    SceneDashboardAuxiliaryReadoutResult result{};
    result.scaled_value = static_cast<float>(
        static_cast<long double>(record_value) *
        static_cast<long double>(global_multiplier));
    result.truncated_value = truncate_to_low_i32(result.scaled_value);
    result.numeric_text = std::to_string(result.truncated_value);
    if (result.numeric_text.size() > 3) {
        // The callback copies the fixed "999" string when the formatted
        // integer has more than three characters, including a minus sign.
        result.numeric_text = "999";
    }
    const auto vertical_advance = static_cast<std::int64_t>(
        observation.zero_glyph_advance) *
        static_cast<std::int64_t>(result.numeric_text.size());
    result.numeric_text_x = static_cast<std::int32_t>(0x22A - vertical_advance);

    if (observation.redraw_all) {
        result.heading_text = observation.global_text_0x18;
        result.secondary_text = observation.stack_text_after_esp_0x18_plus_5;
        result.calls.push_back({0x0040434A, 560, 45, 0x004CFDFC,
                                0x004EB314, 0x004EB2A8, 0x21});
    }
    result.calls.push_back({0x004043E8, result.numeric_text_x, 45,
                            0x004CFE0C, 0x004EB314, 0x004EB304, 0x21});
    if (observation.redraw_all) {
        result.calls.push_back({0x00404418, 513, 22, 0x004CFE10,
                                0x004EB488, 0x004EB41C, 0x21});
    }
    return result;
}

void update_scene_graphics_overlay(SceneGraphicsOverlayState& state,
                                   const std::uint16_t key) noexcept
{
    switch (key) {
    case 0x27:
        state.lod_factor_0x4d11d4 = 1.0F;
        break;
    case 0x5B:
        state.lod_factor_0x4d11d4 -= 0.2F;
        break;
    case 0x5D:
        state.lod_factor_0x4d11d4 += 0.2F;
        break;
    case 0x66:
        state.action_0x40_enabled = !state.action_0x40_enabled;
        if (state.action_0x40_enabled) {
            state.active_action_mask_0x509f50 |= 0x40;
        } else {
            state.active_action_mask_0x509f50 &= ~std::uint32_t{0x40};
        }
        break;
    case 0x6C:
        state.action_0x01_enabled = !state.action_0x01_enabled;
        if (state.action_0x01_enabled) {
            state.active_action_mask_0x509f50 |= 0x01;
        } else {
            state.active_action_mask_0x509f50 &= ~std::uint32_t{0x01};
        }
        break;
    default:
        break;
    }

    constexpr auto maximum_bits = std::bit_cast<std::int32_t>(20.0F);
    constexpr auto minimum_bits = std::bit_cast<std::int32_t>(0.2F);
    if (std::bit_cast<std::int32_t>(state.lod_factor_0x4d11d4) > maximum_bits) {
        state.lod_factor_0x4d11d4 = 20.0F;
    }
    if (std::bit_cast<std::int32_t>(state.lod_factor_0x4d11d4) < minimum_bits) {
        state.lod_factor_0x4d11d4 = 0.2F;
    }
}

std::vector<SceneOverlayTextCall> scene_graphics_overlay_text_plan(
    const SceneGraphicsOverlayState& state)
{
    constexpr std::uint32_t active_color = 0x004ECCFC;
    constexpr std::uint32_t inactive_color = 0x004ECCF0;
    return {
        {10, 10, 0x004D11D8, 0x004ECCD8, -1},
        {10, 20, 0x004D11EC, 0x004ECCC4, -1},
        {160, 10, 0x004D1200,
         state.action_0x40_enabled ? active_color : inactive_color, -1},
        {160, 20, 0x004D1204,
         state.action_0x01_enabled ? active_color : inactive_color, -1},
    };
}

std::vector<SceneGraphicsOverlayOperation> scene_graphics_overlay_render_plan(
    const SceneGraphicsOverlayState& state)
{
    constexpr std::uint32_t active_color = 0x004ECCFC;
    constexpr std::uint32_t inactive_color = 0x004ECCF0;
    return {
        {SceneGraphicsOverlayOperationKind::background_fill_0x44a1b0,
         1, 1, 0U, 0x004ECCC8U, -1, 0.0F},
        {SceneGraphicsOverlayOperationKind::formatted_lod_text_0x44d6a0,
         10, 10, 0x004D11D8U, 0x004ECCD8U, -1,
         state.lod_factor_0x4d11d4},
        {SceneGraphicsOverlayOperationKind::static_text_0x44d6a0,
         10, 20, 0x004D11ECU, 0x004ECCC4U, -1, 0.0F},
        {SceneGraphicsOverlayOperationKind::static_text_0x44d6a0,
         160, 10, 0x004D1200U,
         state.action_0x40_enabled ? active_color : inactive_color, -1, 0.0F},
        {SceneGraphicsOverlayOperationKind::static_text_0x44d6a0,
         160, 20, 0x004D1204U,
         state.action_0x01_enabled ? active_color : inactive_color, -1, 0.0F},
    };
}

std::vector<SceneRectangleFillPixelWrite>
scene_graphics_overlay_background_pixel_writes(
    const RenderContextViewport& context,
    const std::uint8_t renderer_mode,
    const std::uint32_t framebuffer_base,
    const std::uint32_t background_color)
{
    return scene_rectangle_fill_pixel_writes(
        1, 1, 0xE6, 0x22, background_color, context, renderer_mode,
        framebuffer_base);
}

SceneGraphicsOverlayRasterPlan raster_scene_graphics_overlay(
    const SceneGraphicsOverlayRasterObservation& observation)
{
    SceneGraphicsOverlayRasterPlan result;
    result.background_writes = scene_graphics_overlay_background_pixel_writes(
        observation.render_context, observation.renderer_mode,
        observation.framebuffer_base, observation.background_color);

    const auto raster_text = [&](const std::string_view text,
                                 const std::int32_t x,
                                 const std::int32_t y,
                                 const std::uint32_t color) {
        const SceneDashboardBuiltinFontObservation font_observation{
            text, x, y, color,
            {observation.renderer_mode,
             observation.render_context.viewport_left_0x14,
             observation.render_context.viewport_top_0x18,
             observation.render_context.viewport_right_0x1c,
             observation.render_context.viewport_bottom_0x20,
             observation.render_context.row_pitch_0x10,
             observation.framebuffer_base, {}}};
        const auto plan = raster_scene_dashboard_builtin_font(font_observation);
        result.input_complete = result.input_complete && plan.input_complete;
        result.ordered_text_writes.insert(result.ordered_text_writes.end(),
            plan.pixel_writes.begin(), plan.pixel_writes.end());
    };

    raster_text(observation.lod_text, 10, 10,
                observation.lod_text_color);
    raster_text("[=down ]=up '=1.0", 10, 20,
                observation.instruction_text_color);
    raster_text("FOG", 160, 10,
        observation.state.action_0x40_enabled
            ? observation.active_action_color
            : observation.inactive_action_color);
    raster_text("LIGHT", 160, 20,
        observation.state.action_0x01_enabled
            ? observation.active_action_color
            : observation.inactive_action_color);
    return result;
}

SceneRectangleFillApplyResult apply_scene_graphics_overlay_raster(
    const SceneGraphicsOverlayRasterPlan& plan,
    const std::span<std::uint8_t> framebuffer)
{
    if (!plan.input_complete) {
        return {};
    }
    auto writes = plan.background_writes;
    writes.reserve(writes.size() + plan.ordered_text_writes.size());
    for (const auto& text_write : plan.ordered_text_writes) {
        if (text_write.offset_from_framebuffer_base < 0 ||
            static_cast<std::uint64_t>(text_write.offset_from_framebuffer_base) >
                std::numeric_limits<std::uint32_t>::max()) {
            return {};
        }
        const auto mask = text_write.bytes_per_pixel == 4U
            ? 0xFFFFFFFFU : 0xFFFFU;
        writes.push_back({
            text_write.x, text_write.y,
            static_cast<std::uint32_t>(text_write.offset_from_framebuffer_base),
            text_write.destination_address, text_write.value, mask,
            text_write.bytes_per_pixel, false});
    }
    return apply_scene_rectangle_fill_pixel_writes(writes, framebuffer);
}

} // namespace viper
