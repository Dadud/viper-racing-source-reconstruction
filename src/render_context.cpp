#include "viper/render_context.hpp"

namespace viper {

void initialize_render_context_from_surface(
    RenderContextSurface& context, const std::uint8_t format_tag,
    const std::uintptr_t surface_bits, const int width, const int height,
    const int row_pitch) noexcept
{
    context.format_tag_0x00 = format_tag;
    context.flags_0x01 = 0x80U;
    context.surface_bits_0x04 = surface_bits;
    context.viewport.surface_width_0x08 = width;
    context.viewport.surface_height_0x0c = height;
    context.viewport.row_pitch_0x10 = row_pitch;
    reset_render_context_viewport(context.viewport);
}

void set_render_context_viewport(RenderContextViewport& context,
                                int left, int top,
                                const int right,
                                const int bottom) noexcept
{
    context.viewport_left_0x14 = left < 1 ? 0 : left;
    context.viewport_top_0x18 = top < 1 ? 0 : top;
    context.viewport_right_0x1c = right <= context.surface_width_0x08
        ? right : context.surface_width_0x08;
    context.viewport_bottom_0x20 = bottom <= context.surface_height_0x0c
        ? bottom : context.surface_height_0x0c;
}

void reset_render_context_viewport(RenderContextViewport& context) noexcept
{
    context.viewport_left_0x14 = 0;
    context.viewport_top_0x18 = 0;
    context.viewport_right_0x1c = context.surface_width_0x08;
    context.viewport_bottom_0x20 = context.surface_height_0x0c;
}

bool render_context_contains_pixel(const RenderContextViewport& context,
                                   const int x, const int y) noexcept
{
    return x >= context.viewport_left_0x14
        && y >= context.viewport_top_0x18
        && x < context.viewport_right_0x1c
        && y < context.viewport_bottom_0x20;
}

RenderPixelEncoding encode_render_pixel(const std::uint8_t raw_format_tag,
                                        const std::uint32_t source_color) noexcept
{
    if (raw_format_tag == 5U) {
        const auto component_0 = ((source_color >> 19U) & 0xffU) & 0xfcU;
        const auto component_1 = ((source_color >> 24U) & 0xffU) & 0xf8U;
        const auto component_2 = ((source_color >> 13U) & 0xffU) & 0xf8U;
        const auto bits = ((component_0 | 0xffff0000U) << 8U)
            | (component_1 << 16U)
            | component_2;
        return {bits, 4U};
    }

    const auto word = raw_format_tag == 3U
        ? (source_color >> 16U) & 0xffffU
        : source_color & 0xffffU;
    return {word, 2U};
}

std::uint32_t render_context_pixel_offset(
    const RenderContextViewport& context,
    const std::uint8_t raw_format_tag,
    const int x,
    const int y) noexcept
{
    const std::uint32_t bytes_per_pixel = raw_format_tag == 5U ? 4U : 2U;
    return static_cast<std::uint32_t>(y)
            * static_cast<std::uint32_t>(context.row_pitch_0x10)
        + static_cast<std::uint32_t>(x) * bytes_per_pixel;
}

} // namespace viper
