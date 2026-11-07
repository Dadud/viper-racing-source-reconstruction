#include "viper/texture_resource.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

namespace {

void write_u32(std::vector<std::byte>& bytes, const std::size_t offset,
               const std::uint32_t value)
{
    for (std::size_t index = 0U; index < 4U; ++index)
        bytes[offset + index] = static_cast<std::byte>(value >> (index * 8U));
}

void write_u16(std::vector<std::byte>& bytes, const std::size_t offset,
               const std::uint16_t value)
{
    bytes[offset] = static_cast<std::byte>(value & 0xffU);
    bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
}

bool check(const bool condition, const char* message)
{
    if (condition) return true;
    std::cerr << message << '\n';
    return false;
}

} // namespace

int main()
{
    // A compact 4x4 RGB565 TEX: header, 28 reserved bytes, padded 2x2 level,
    // then the full 4x4 level. The 1x1 level is duplicated in header +0x0c.
    std::vector<std::byte> payload(108U);
    payload[0] = std::byte{0U};
    payload[1] = std::byte{1U};
    payload[3] = std::byte{1U};
    write_u32(payload, 8U, 3U);
    write_u16(payload, 12U, 0xf800U);
    write_u16(payload, 44U, 0x07e0U); // 2x2 row 0, column 0: green
    write_u16(payload, 46U, 0x001fU); // 2x2 row 0, column 1: blue
    write_u16(payload, 48U, 0xffffU); // padded column, must be ignored
    write_u16(payload, 52U, 0xf800U); // 2x2 row 1, column 0: red
    write_u16(payload, 54U, 0x0000U); // 2x2 row 1, column 1: black
    for (std::size_t pixel = 0U; pixel < 16U; ++pixel)
        write_u16(payload, 76U + pixel * 2U, 0x07e0U);

    const auto decoded = viper::parse_texture_payload(payload);
    if (!check(decoded.has_value(), "valid mip-chain payload was rejected")) return 1;
    if (!check(decoded->mip_rgba.size() == 3U, "mip level count was not preserved")) return 1;
    if (!check(decoded->mip_rgba[0].size() == 64U &&
            decoded->mip_rgba[1].size() == 16U &&
            decoded->mip_rgba[2].size() == 4U,
            "decoded mip dimensions are wrong")) return 1;
    if (!check(decoded->mip_rgba[0] == decoded->rgba,
            "legacy base-level view does not match mip zero")) return 1;
    constexpr std::array<std::uint8_t, 4> green{0U, 248U, 0U, 255U};
    constexpr std::array<std::uint8_t, 4> blue{0U, 0U, 248U, 255U};
    constexpr std::array<std::uint8_t, 4> red{248U, 0U, 0U, 255U};
    if (!check(std::equal(green.begin(), green.end(), decoded->mip_rgba[1].begin()),
            "2x2 padded level row 0, column 0 decoded incorrectly")) return 1;
    if (!check(std::equal(blue.begin(), blue.end(), decoded->mip_rgba[1].begin() + 4),
            "2x2 padded level row 0, column 1 decoded incorrectly")) return 1;
    if (!check(std::equal(red.begin(), red.end(), decoded->mip_rgba[1].begin() + 8),
            "2x2 padded level row 1, column 0 decoded incorrectly")) return 1;
    if (!check(std::equal(red.begin(), red.end(), decoded->mip_rgba[2].begin()),
            "header-duplicated 1x1 mip decoded incorrectly")) return 1;

    payload.pop_back();
    if (!check(!viper::parse_texture_payload(payload),
            "truncated mip chain was accepted")) return 1;
    std::cout << "texture resource regression checks passed\n";
    return 0;
}
