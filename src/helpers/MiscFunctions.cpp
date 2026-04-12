#include "MiscFunctions.hpp"
#include "Log.hpp"
#include "../core/hyprlock.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <hyprutils/os/Process.hpp>
#include <hyprutils/string/String.hpp>
#include <pwd.h>
#include <unistd.h>

using namespace Hyprutils::String;
using namespace Hyprutils::OS;

std::string absolutePath(const std::string& rawpath, const std::string& currentDir) {
    std::filesystem::path path(rawpath);

    // Handling where rawpath starts with '~'
    if (!rawpath.empty() && rawpath[0] == '~') {
        static const char* const ENVHOME = getenv("HOME");
        path                             = std::filesystem::path(ENVHOME) / path.relative_path().string().substr(2);
    }

    // Handling e.g. ./, ../
    if (path.is_relative()) {
        return std::filesystem::weakly_canonical(std::filesystem::path(currentDir) / path);
    } else {
        return std::filesystem::weakly_canonical(path);
    }
}

int64_t configStringToInt(const std::string& VALUE) {
    auto parseHex = [](const std::string& value) -> int64_t {
        try {
            size_t position;
            auto   result = stoll(value, &position, 16);
            if (position == value.size())
                return result;
        } catch (const std::exception&) {}
        throw std::invalid_argument("invalid hex " + value);
    };
    if (VALUE.starts_with("0x")) {
        // Values with 0x are hex
        return parseHex(VALUE);
    } else if (VALUE.starts_with("rgba(") && VALUE.ends_with(')')) {
        const auto VALUEWITHOUTFUNC = trim(VALUE.substr(5, VALUE.length() - 6));

        // try doing it the comma way first
        if (std::count(VALUEWITHOUTFUNC.begin(), VALUEWITHOUTFUNC.end(), ',') == 3) {
            // cool
            std::string rolling = VALUEWITHOUTFUNC;
            auto        r       = configStringToInt(trim(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            auto g              = configStringToInt(trim(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            auto b              = configStringToInt(trim(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            uint8_t a           = 0;
            try {
                a = std::round(std::stof(trim(rolling.substr(0, rolling.find(',')))) * 255.f);
            } catch (std::exception& e) { throw std::invalid_argument("failed parsing " + VALUEWITHOUTFUNC); }

            return (a * (Hyprlang::INT)0x1000000) + (r * (Hyprlang::INT)0x10000) + (g * (Hyprlang::INT)0x100) + b;
        } else if (VALUEWITHOUTFUNC.length() == 8) {
            const auto RGBA = parseHex(VALUEWITHOUTFUNC);
            // now we need to RGBA -> ARGB. The config holds ARGB only.
            return (RGBA >> 8) + (0x1000000 * (RGBA & 0xFF));
        }

        throw std::invalid_argument("rgba() expects length of 8 characters (4 bytes) or 4 comma separated values");

    } else if (VALUE.starts_with("rgb(") && VALUE.ends_with(')')) {
        const auto VALUEWITHOUTFUNC = trim(VALUE.substr(4, VALUE.length() - 5));

        // try doing it the comma way first
        if (std::count(VALUEWITHOUTFUNC.begin(), VALUEWITHOUTFUNC.end(), ',') == 2) {
            // cool
            std::string rolling = VALUEWITHOUTFUNC;
            auto        r       = configStringToInt(trim(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            auto g              = configStringToInt(trim(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            auto b              = configStringToInt(trim(rolling.substr(0, rolling.find(','))));

            return (Hyprlang::INT)0xFF000000 + (r * (Hyprlang::INT)0x10000) + (g * (Hyprlang::INT)0x100) + b;
        } else if (VALUEWITHOUTFUNC.length() == 6) {
            return parseHex(VALUEWITHOUTFUNC) + 0xFF000000;
        }

        throw std::invalid_argument("rgb() expects length of 6 characters (3 bytes) or 3 comma separated values");
    } else if (VALUE.starts_with("true") || VALUE.starts_with("on") || VALUE.starts_with("yes")) {
        return 1;
    } else if (VALUE.starts_with("false") || VALUE.starts_with("off") || VALUE.starts_with("no")) {
        return 0;
    }

    if (VALUE.empty() || !isNumber(VALUE, false))
        throw std::invalid_argument("cannot parse \"" + VALUE + "\" as an int.");

    try {
        const auto RES = std::stoll(VALUE);
        return RES;
    } catch (std::exception& e) { throw std::invalid_argument(std::string{"stoll threw: "} + e.what()); }

    return 0;
}

int createPoolFile(size_t size, std::string& name) {
    const auto XDGRUNTIMEDIR = getenv("XDG_RUNTIME_DIR");
    if (!XDGRUNTIMEDIR) {
        Log::logger->log(Log::CRIT, "XDG_RUNTIME_DIR not set!");
        return -1;
    }

    name = std::string(XDGRUNTIMEDIR) + "/.hyprlogin_sc_XXXXXX";

    const auto FD = mkstemp((char*)name.c_str());
    if (FD < 0) {
        Log::logger->log(Log::CRIT, "createPoolFile: fd < 0");
        return -1;
    }
    // set cloexec
    long flags = fcntl(FD, F_GETFD);
    if (flags == -1) {
        close(FD);
        return -1;
    }

    if (fcntl(FD, F_SETFD, flags | FD_CLOEXEC) == -1) {
        close(FD);
        Log::logger->log(Log::CRIT, "createPoolFile: fcntl < 0");
        return -1;
    }

    if (ftruncate(FD, size) < 0) {
        close(FD);
        Log::logger->log(Log::CRIT, "createPoolFile: ftruncate < 0");
        return -1;
    }

    return FD;
}

std::string spawnSync(const std::string& cmd) {
    CProcess proc("/bin/sh", {"-c", cmd});
    if (!proc.runSync()) {
        Log::logger->log(Log::ERR, "Failed to run \"{}\"", cmd);
        return "";
    }

    if (!proc.stdErr().empty())
        Log::logger->log(Log::ERR, "Shell command \"{}\" STDERR:\n{}", cmd, proc.stdErr());

    return proc.stdOut();
}

void spawnAsync(const std::string& cmd) {
    CProcess proc("/bin/sh", {"-c", cmd});
    if (!proc.runAsync())
        Log::logger->log(Log::ERR, "Failed to start \"{}\"", cmd);
}

std::string getUsernameForCurrentUid() {
    const uid_t UID         = getuid();
    auto        uidPassword = getpwuid(UID);
    if (!uidPassword || !uidPassword->pw_name) {
        Log::logger->log(Log::ERR, "Failed to get username for uid {} (getpwuid)", UID);
        return "";
    }

    return std::string{uidPassword->pw_name};
}

bool isExecutableCommand(const std::string& exec) {
    if (exec.empty())
        return false;

    auto firstToken = exec.substr(0, exec.find_first_of(" \t"));
    if (firstToken.empty())
        return false;

    while (firstToken.contains("=") && exec.find_first_of(" \t") != std::string::npos) {
        const auto nextPos = exec.find_first_not_of(" \t", exec.find_first_of(" \t"));
        if (nextPos == std::string::npos)
            return false;

        const auto rest = exec.substr(nextPos);
        firstToken      = rest.substr(0, rest.find_first_of(" \t"));
        if (!firstToken.contains("="))
            break;
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

bool handleInternalCommand(const std::string& cmd) {
    if (cmd == "hyprlogin:session_next") {
        g_pHyprlock->cycleSession(1);
        g_pHyprlock->renderAllOutputs();
        return true;
    }

    if (cmd == "hyprlogin:session_prev") {
        g_pHyprlock->cycleSession(-1);
        g_pHyprlock->renderAllOutputs();
        return true;
    }

    if (cmd == "hyprlogin:clear_input") {
        g_pHyprlock->clearPasswordBuffer();
        return true;
    }

    return false;
}
