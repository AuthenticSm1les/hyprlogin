#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct SGreeterSession {
    std::string name;
    std::string command;
    std::string type; // "wayland" or "x11"
    std::string desktopNames;
    std::string desktopFile;
};

namespace Sessions {
    // Reads the .desktop session files in both directories, skipping hidden
    // entries and ones whose TryExec/Exec binary is missing. Adds a startx
    // entry when startx is installed. Sorted by name, duplicates removed.
    std::vector<SGreeterSession> discover(const std::string& waylandPath, const std::string& x11Path);

    std::optional<SGreeterSession> parseDesktopFile(const std::string& path, const std::string& type);

    // Matches a session by name, command, .desktop path, file name or stem.
    bool matches(const SGreeterSession& session, std::string_view id);

    // Stable identifier to persist for a session.
    std::string id(const SGreeterSession& session);

    // XDG_SESSION_TYPE / XDG_SESSION_DESKTOP / DESKTOP_SESSION / XDG_CURRENT_DESKTOP.
    std::vector<std::string> environment(const SGreeterSession& session);

    // True if the first non-assignment token of exec resolves to an executable.
    bool isExecutableCommand(const std::string& exec);
}
