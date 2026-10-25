#include "viper/network_race_input.hpp"

#include <bit>
#include <cmath>
#include <stdexcept>

namespace viper::net {
namespace {
void write_float(std::uint8_t* output, const float value)
{
    const auto bits = std::bit_cast<std::uint32_t>(value);
    for (unsigned i = 0; i < 4; ++i)
        output[i] = static_cast<std::uint8_t>(bits >> ((3U - i) * 8U));
}

float read_float(const std::uint8_t* input) noexcept
{
    std::uint32_t bits{};
    for (unsigned i = 0; i < 4; ++i)
        bits |= static_cast<std::uint32_t>(input[i]) << ((3U - i) * 8U);
    return std::bit_cast<float>(bits);
}
}

std::array<std::uint8_t, race_input_payload_size> encode_race_input(
    const RaceInput& input)
{
    if (!std::isfinite(input.steering) || !std::isfinite(input.throttle) ||
        !std::isfinite(input.brake) || input.steering < -1.0F ||
        input.steering > 1.0F || input.throttle < 0.0F ||
        input.throttle > 1.0F || input.brake < 0.0F || input.brake > 1.0F)
        throw std::invalid_argument("race input is outside normalized bounds");
    std::array<std::uint8_t, race_input_payload_size> bytes{};
    write_float(bytes.data(), input.steering);
    write_float(bytes.data() + 4, input.throttle);
    write_float(bytes.data() + 8, input.brake);
    return bytes;
}

bool decode_race_input(const std::span<const std::uint8_t> bytes,
                       RaceInput& input) noexcept
{
    if (bytes.size() != race_input_payload_size) return false;
    const RaceInput decoded{read_float(bytes.data()), read_float(bytes.data() + 4),
        read_float(bytes.data() + 8)};
    if (!std::isfinite(decoded.steering) || !std::isfinite(decoded.throttle) ||
        !std::isfinite(decoded.brake) || decoded.steering < -1.0F ||
        decoded.steering > 1.0F || decoded.throttle < 0.0F ||
        decoded.throttle > 1.0F || decoded.brake < 0.0F || decoded.brake > 1.0F)
        return false;
    input = decoded;
    return true;
}

} // namespace viper::net
