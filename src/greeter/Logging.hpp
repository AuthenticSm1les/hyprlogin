#pragma once

#include <expected>
#include <string>

#include <hyprutils/cli/ArgumentParser.hpp>

// hyprlogin's debug logging switches. They are command-line flags (with
// environment variable fallbacks, since greetd starts the greeter through
// hyprland-greeter.lua) so they take effect before the config is parsed.
//
//   -d, --debug        / HYPRLOGIN_DEBUG=1      [debug] messages, implies --verbose
//   --log-file <path>  / HYPRLOGIN_LOG_FILE     also write the log to <path>;
//                                              with --debug it defaults to DEFAULT_LOG_FILE
#ifndef HYPRLOGIN_CACHE_DIR
#define HYPRLOGIN_CACHE_DIR "/var/cache/hyprlogin"
#endif

namespace GreeterLogging {
    inline constexpr const char* DEBUG_ENV        = "HYPRLOGIN_DEBUG";
    inline constexpr const char* LOG_FILE_ENV     = "HYPRLOGIN_LOG_FILE";
    inline constexpr const char* DEFAULT_LOG_FILE = HYPRLOGIN_CACHE_DIR "/debug.log";

    void registerArgs(Hyprutils::CLI::CArgumentParser& parser);

    // Applies the flags (or their environment variables) to Log::logger.
    // A log file that can't be opened is reported and otherwise ignored.
    void setup(Hyprutils::CLI::CArgumentParser& parser);

    // Opens <path> for appending. Creates it with mode 0600 if missing, or
    // checks that an existing one is a regular file owned by us (symlinks are
    // refused) and tightens it to 0600. Returns the fd (O_CLOEXEC).
    std::expected<int, std::string> openLogFile(const std::string& path);
}
