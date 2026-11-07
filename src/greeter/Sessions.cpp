#include "Sessions.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <unistd.h>

static std::string trimWhitespace(std::string value) {
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), value.end());
    return value;
}

// Drops desktop-entry field codes (%f, %U, ...), which make no sense for a session.
static std::string sanitizeDesktopExec(const std::string& exec) {
    std::string sanitized;
    sanitized.reserve(exec.size());

    for (size_t i = 0; i < exec.size(); ++i) {
        if (exec[i] == '%' && i + 1 < exec.size() && std::isalpha(static_cast<unsigned char>(exec[i + 1]))) {
            ++i;
            continue;
        }

        sanitized.push_back(exec[i]);
    }

    return trimWhitespace(sanitized);
}

static std::string getDesktopEntryValue(const std::filesystem::path& path, const std::string& key) {
    std::ifstream file(path);
    if (!file.is_open())
        return "";

    std::string line;
    bool        inDesktopEntry = false;
    while (std::getline(file, line)) {
        line = trimWhitespace(line);
        if (line.empty() || line.starts_with("#"))
            continue;

        if (line == "[Desktop Entry]") {
            inDesktopEntry = true;
            continue;
        }

        if (!inDesktopEntry)
            continue;

        if (line.starts_with('['))
            break;

        const auto pos = line.find('=');
        if (pos == std::string::npos)
            continue;

        if (line.substr(0, pos) == key)
            return trimWhitespace(line.substr(pos + 1));
    }

    return "";
}

std::optional<SGreeterSession> Sessions::parseDesktopFile(const std::string& path, const std::string& type) {
    const auto hidden    = getDesktopEntryValue(path, "Hidden");
    const auto noDisplay = getDesktopEntryValue(path, "NoDisplay");
    const auto name      = getDesktopEntryValue(path, "Name");
    const auto tryExec   = getDesktopEntryValue(path, "TryExec");
    const auto exec      = sanitizeDesktopExec(getDesktopEntryValue(path, "Exec"));
    const auto desktops  = getDesktopEntryValue(path, "DesktopNames");

    if (hidden == "true" || noDisplay == "true")
        return std::nullopt;

    if (name.empty() || exec.empty())
        return std::nullopt;

    if (!tryExec.empty() && !isExecutableCommand(tryExec))
        return std::nullopt;

    if (!isExecutableCommand(exec))
        return std::nullopt;

    return SGreeterSession{
        .name         = name,
        .command      = exec,
        .type         = type,
        .desktopNames = desktops,
        .desktopFile  = path,
    };
}

std::vector<SGreeterSession> Sessions::discover(const std::string& waylandPath, const std::string& x11Path) {
    std::vector<SGreeterSession> sessions;

    const auto loadPath = [&sessions](const std::string& directory, const std::string& type) {
        std::error_code ec;
        if (directory.empty() || !std::filesystem::is_directory(directory, ec))
            return;

        for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".desktop")
                continue;

            if (auto session = parseDesktopFile(entry.path().string(), type); session.has_value())
                sessions.emplace_back(std::move(*session));
        }
    };

    loadPath(waylandPath, "wayland");
    loadPath(x11Path, "x11");

    if (isExecutableCommand("startx")) {
        sessions.emplace_back(SGreeterSession{
            .name         = "Default X11 Session",
            .command      = "startx",
            .type         = "x11",
            .desktopNames = "",
            .desktopFile  = "/usr/share/xsessions/startx.desktop",
        });
    }

    std::ranges::sort(sessions, [](const auto& a, const auto& b) { return a.name < b.name; });
    const auto [first, last] =
        std::ranges::unique(sessions, [](const auto& a, const auto& b) { return a.name == b.name && a.command == b.command && a.type == b.type; });
    sessions.erase(first, last);

    return sessions;
}

bool Sessions::matches(const SGreeterSession& session, std::string_view id) {
    if (session.name == id || session.command == id || session.desktopFile == id)
        return true;

    const auto desktopFileName = std::filesystem::path(session.desktopFile).filename().string();
    if (desktopFileName == id)
        return true;

    auto desktopFileStem = std::filesystem::path(desktopFileName);
    desktopFileStem.replace_extension("");
    return desktopFileStem.string() == id;
}

std::string Sessions::id(const SGreeterSession& session) {
    return session.desktopFile.empty() ? session.name : session.desktopFile;
}

std::vector<std::string> Sessions::environment(const SGreeterSession& session) {
    std::vector<std::string> env;
    env.emplace_back(std::format("XDG_SESSION_TYPE={}", session.type == "x11" ? "x11" : "wayland"));
    env.emplace_back(std::format("XDG_SESSION_DESKTOP={}", session.desktopNames.empty() ? session.name : session.desktopNames));
    auto desktopFilePath = std::filesystem::path(session.desktopFile).filename();
    desktopFilePath.replace_extension("");
    env.emplace_back(std::format("DESKTOP_SESSION={}", desktopFilePath.string()));
    if (!session.desktopNames.empty()) {
        // DesktopNames is ';'-separated, XDG_CURRENT_DESKTOP is ':'-separated.
        std::string desktopNamesColon = session.desktopNames;
        std::ranges::replace(desktopNamesColon, ';', ':');

        if (!desktopNamesColon.empty() && desktopNamesColon.back() == ':')
            desktopNamesColon.pop_back();
        env.emplace_back(std::format("XDG_CURRENT_DESKTOP={}", desktopNamesColon));
    }

    return env;
}

bool Sessions::isExecutableCommand(const std::string& exec) {
    if (exec.empty())
        return false;

    size_t pos = exec.find_first_not_of(" \t");
    if (pos == std::string::npos)
        return false;

    // Skip leading VAR=value assignments ("env-style" Exec lines).
    std::string firstToken;
    while (true) {
        const auto end = exec.find_first_of(" \t", pos);
        firstToken     = exec.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        if (firstToken.empty())
            return false;
        if (!firstToken.contains("="))
            break;
        if (end == std::string::npos)
            return false;
        pos = exec.find_first_not_of(" \t", end);
        if (pos == std::string::npos)
            return false;
    }

    if (firstToken.starts_with('/'))
        return access(firstToken.c_str(), R_OK | X_OK) == 0;

    const auto* pathEnv = getenv("PATH");
    if (!pathEnv)
        return false;

    std::string paths = pathEnv;
    size_t      start = 0;
    while (start <= paths.size()) {
        const auto end   = paths.find(':', start);
        const auto entry = paths.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!entry.empty()) {
            const auto candidate = std::filesystem::path(entry) / firstToken;
            if (std::filesystem::exists(candidate) && access(candidate.c_str(), R_OK | X_OK) == 0)
                return true;
        }

        if (end == std::string::npos)
            break;
        start = end + 1;
    }

    return false;
}
