#include "viper/game_options.hpp"

#include <cmath>
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
        const auto options = viper::GameOptions::parse(
            "version 1\n[GLOBAL]\ntrack_no 7\ncar_no 0\n"
            "[GAME]\nlaps 1\nai_cars 7\ndamage_on yes\n"
            "[CONTROL]\nsteer_sensitivity 1.000000\n"
            "look_left \n"
            "[GLOBAL]\ntrack_no 2\n");
        require(options.integer("global", "track_no") == 2,
            "later sections must retain their later key values");
        require(options.integer("global", "version") == 1,
            "version preamble must be retained as global configuration");
        require(options.integer("GLOBAL", "car_no") == 0,
            "section names and keys must be case insensitive");
        require(options.integer("game", "laps") == 1 &&
            options.integer("game", "ai_cars") == 7,
            "integer settings must parse");
        require(options.boolean("game", "damage_on") == true,
            "yes/no values must parse as booleans");
        require(options.get("control", "look_left") == std::string_view{},
            "empty binding values must be preserved");
        const auto sensitivity = options.real("control", "steer_sensitivity");
        require(sensitivity && std::abs(*sensitivity - 1.0F) < 1.0e-6F,
            "floating point settings must parse");
        require(!options.integer("game", "damage_on"),
            "non-numeric option values must not parse as integers");
        bool malformed_rejected = false;
        try {
            (void)viper::GameOptions::parse("[GAME\nlaps 2\n");
        } catch (const std::exception&) {
            malformed_rejected = true;
        }
        require(malformed_rejected, "malformed sections must be rejected");
        std::cout << "game options regression checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "game options regression failure: " << error.what() << '\n';
        return 1;
    }
}
