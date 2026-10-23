#pragma once

#include <cstdint>

namespace viper {

struct RenderContextViewport {
    int surface_width_0x08{};
    int surface_height_0x0c{};
    int row_pitch_0x10{};
    int viewport_left_0x14{};
    int viewport_top_0x18{};
    int viewport_right_0x1c{};
    int viewport_bottom_0x20{};
};

// Surface fields installed by 0x449350 after the renderer acquires a page.
// The binary stores these at context offsets +0x00 through +0x10.
struct RenderContextSurface {
    std::uint8_t format_tag_0x00{};
    std::uint8_t flags_0x01{0x80};
    std::uintptr_t surface_bits_0x04{};
    RenderContextViewport viewport{};
};

// Reproduces the field population performed by 0x449350; viewport reset is
// delegated to the same 0x4498E0 behavior used by the original function.
void initialize_render_context_from_surface(
    RenderContextSurface& context, std::uint8_t format_tag,
    std::uintptr_t surface_bits, int width, int height, int row_pitch) noexcept;

struct RenderPixelEncoding {
    std::uint32_t bits{};
    std::uint8_t bytes_per_pixel{};
};

// Reproduces the coordinate clamp and upper-size limits in 0x4498A0.
void set_render_context_viewport(RenderContextViewport& context,
                                int left, int top,
                                int right, int bottom) noexcept;

// Reproduces the full-surface reset in 0x4498E0.
void reset_render_context_viewport(RenderContextViewport& context) noexcept;

// Reproduces the half-open viewport test in 0x4499B0.
[[nodiscard]] bool render_context_contains_pixel(
    const RenderContextViewport& context, int x, int y) noexcept;

// Reproduces the tag-selected word/dword conversion in 0x449E90.
[[nodiscard]] RenderPixelEncoding encode_render_pixel(
    std::uint8_t raw_format_tag, std::uint32_t source_color) noexcept;

// Returns y*pitch+x*bytes-per-pixel, the byte offset from surface bits at +4.
[[nodiscard]] std::uint32_t render_context_pixel_offset(
    const RenderContextViewport& context,
    std::uint8_t raw_format_tag, int x, int y) noexcept;

} // namespace viper
