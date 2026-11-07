#include "RememberCache.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

namespace {
    // Cached values are usernames and session identifiers; anything longer or
    // containing control characters is treated as corrupt.
    constexpr size_t MAX_VALUE_LEN = 4096;

    bool isValidValue(const std::string& value) {
        if (value.empty() || value.size() > MAX_VALUE_LEN)
            return false;

        return std::ranges::none_of(value, [](unsigned char c) { return c < 0x20 || c == 0x7f; });
    }
}

std::optional<std::string> RememberCache::read(const std::string& dir, const std::string& entry) {
    if (dir.empty())
        return std::nullopt;

    std::ifstream in(std::filesystem::path{dir} / entry, std::ios::binary);
    if (!in.is_open())
        return std::nullopt;

    std::string value{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r'))
        value.pop_back();

    if (!isValidValue(value))
        return std::nullopt;

    return value;
}

bool RememberCache::write(const std::string& dir, const std::string& entry, const std::string& value) {
    if (dir.empty() || !isValidValue(value))
        return false;

    const auto      target = std::filesystem::path{dir} / entry;
    const auto      tmp    = std::filesystem::path{dir} / ("." + entry + ".tmp");
    std::error_code ec;

    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out.is_open())
            return false;

        out << value << '\n';
        out.flush();
        if (!out.good()) {
            out.close();
            std::filesystem::remove(tmp, ec);
            return false;
        }
    }

    std::filesystem::rename(tmp, target, ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        return false;
    }

    return true;
}
