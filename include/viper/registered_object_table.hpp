#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace viper {

inline constexpr std::size_t registered_object_table_count = 16;
using RegisteredObjectTable =
    std::array<std::uint32_t, registered_object_table_count>;

} // namespace viper
