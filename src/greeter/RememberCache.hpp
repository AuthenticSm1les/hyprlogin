#pragma once

#include <optional>
#include <string>

// Persists the last successful login (user and session) between greeter runs,
// similar to tuigreet's --remember / --remember-session. Each entry is a
// single-line file inside the cache directory.
namespace RememberCache {
    inline constexpr const char* USER_ENTRY    = "lastuser";
    inline constexpr const char* SESSION_ENTRY = "lastsession";

    // Returns the stored value, or nullopt if the entry is missing, empty,
    // unreadable or contains control characters.
    std::optional<std::string> read(const std::string& dir, const std::string& entry);

    // Atomically replaces the entry (write to a temp file, then rename).
    // Returns false and leaves the old value in place on any error.
    bool write(const std::string& dir, const std::string& entry, const std::string& value);
}
