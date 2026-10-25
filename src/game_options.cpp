#include "viper/game_options.hpp"

#include <charconv>
#include <string>
#include <cctype>
#include <fstream>
#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace viper {
namespace {

std::string trim(std::string value)
{
    const auto non_space = [](const unsigned char ch) { return !std::isspace(ch); };
    const auto first = std::find_if(value.begin(), value.end(), non_space);
    const auto last = std::find_if(value.rbegin(), value.rend(), non_space).base();
    if (first >= last) return {};
    return std::string(first, last);
}

std::string lower(std::string value)
{
    for (auto& ch : value)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return value;
}

} // namespace

GameOptions GameOptions::load(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("could not open options definition: " + path.string());

    std::string contents((std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    if (input.bad()) throw std::runtime_error("failed while reading options definition");
    return parse(contents);
}

GameOptions GameOptions::parse(const std::string_view contents)
{
    GameOptions options;
    std::string section{"global"};
    std::string line;
    std::size_t line_number{};
    std::size_t cursor{};
    while (cursor <= contents.size()) {
        const auto end = contents.find('\n', cursor);
        line.assign(contents.substr(cursor, end == std::string_view::npos
            ? contents.size() - cursor : end - cursor));
        cursor = end == std::string_view::npos ? contents.size() + 1U : end + 1U;
        ++line_number;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto text = trim(line);
        if (text.empty() || text[0] == ';' || text[0] == '#') continue;
        if (text.front() == '[') {
            if (text.size() < 3U || text.back() != ']')
                throw std::runtime_error("malformed section in options definition at line " +
                    std::to_string(line_number));
            section = lower(trim(text.substr(1U, text.size() - 2U)));
            if (section.empty())
                throw std::runtime_error("empty section in options definition at line " +
                    std::to_string(line_number));
            continue;
        }
        const auto raw_key_start = line.find_first_not_of(" \t");
        const auto raw_separator = raw_key_start == std::string::npos
            ? std::string::npos : line.find_first_of(" \t=", raw_key_start);
        if (raw_separator == std::string::npos)
            throw std::runtime_error("option has no value at line " +
                std::to_string(line_number));
        const auto key = lower(trim(line.substr(raw_key_start,
            raw_separator - raw_key_start)));
        const auto value_start = raw_separator == std::string::npos
            ? std::string::npos : line.find_first_not_of(" \t=", raw_separator);
        options.entries_[section][key] = value_start == std::string::npos
            ? std::string{} : trim(line.substr(value_start));
    }
    return options;
}

std::optional<std::string_view> GameOptions::get(
    const std::string_view section, const std::string_view key) const noexcept
{
    const auto section_it = entries_.find(lower(std::string(section)));
    if (section_it == entries_.end()) return std::nullopt;
    const auto key_it = section_it->second.find(lower(std::string(key)));
    if (key_it == section_it->second.end()) return std::nullopt;
    return key_it->second;
}

std::optional<int> GameOptions::integer(
    const std::string_view section, const std::string_view key) const noexcept
{
    const auto value = get(section, key);
    if (!value) return std::nullopt;
    int result{};
    const auto [end, error] = std::from_chars(value->data(),
        value->data() + value->size(), result);
    if (error != std::errc{} || end != value->data() + value->size())
        return std::nullopt;
    return result;
}

std::optional<float> GameOptions::real(
    const std::string_view section, const std::string_view key) const noexcept
{
    const auto value = get(section, key);
    if (!value) return std::nullopt;
    float result{};
    const auto [end, error] = std::from_chars(value->data(),
        value->data() + value->size(), result);
    if (error != std::errc{} || end != value->data() + value->size())
        return std::nullopt;
    return result;
}

std::optional<bool> GameOptions::boolean(
    const std::string_view section, const std::string_view key) const noexcept
{
    const auto value = get(section, key);
    if (!value) return std::nullopt;
    const auto normalized = lower(std::string(*value));
    if (normalized == "yes" || normalized == "true" || normalized == "1") return true;
    if (normalized == "no" || normalized == "false" || normalized == "0") return false;
    return std::nullopt;
}

void GameOptions::set_integer(std::string section, std::string key, const int value)
{
    entries_[lower(std::move(section))][lower(std::move(key))] = std::to_string(value);
}

} // namespace viper
