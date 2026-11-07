#include "Logging.hpp"
#include "../helpers/Log.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <format>
#include <optional>
#include <string_view>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
    bool envEnabled(const char* name) {
        const char* value = std::getenv(name);
        if (!value)
            return false;

        const std::string_view v{value};
        return !v.empty() && v != "0" && v != "false" && v != "no";
    }

    std::optional<std::string> envString(const char* name) {
        const char* value = std::getenv(name);
        if (!value || !*value)
            return std::nullopt;
        return std::string{value};
    }
}

void GreeterLogging::registerArgs(Hyprutils::CLI::CArgumentParser& parser) {
    ASSERT(parser.registerBoolOption("debug", "d", "Enable greeter debug logging, implies --verbose (env: HYPRLOGIN_DEBUG=1)").has_value());
    ASSERT(parser.registerStringOption("log-file", "", "Also write the log to this file (env: HYPRLOGIN_LOG_FILE)").has_value());
}

void GreeterLogging::setup(Hyprutils::CLI::CArgumentParser& parser) {
    if (parser.getBool("debug").value_or(false) || envEnabled(DEBUG_ENV))
        Log::logger->setDebug();

    std::optional<std::string> logFile;
    if (const auto arg = parser.getString("log-file"); arg.has_value() && !arg->empty())
        logFile = std::string{*arg};
    else
        logFile = envString(LOG_FILE_ENV);

    if (!logFile)
        return;

    const auto fd = openLogFile(*logFile);
    if (!fd) {
        Log::logger->log(Log::ERR, "Not writing log file {}: {}", *logFile, fd.error());
        return;
    }

    Log::logger->setLogFd(*fd);
}

std::expected<int, std::string> GreeterLogging::openLogFile(const std::string& path) {
    const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0)
        return std::unexpected{std::string{strerror(errno)}};

    auto fail = [fd](std::string why) -> std::expected<int, std::string> {
        close(fd);
        return std::unexpected{std::move(why)};
    };

    struct stat st{};
    if (fstat(fd, &st) != 0)
        return fail(strerror(errno));
    if (!S_ISREG(st.st_mode))
        return fail("not a regular file");
    if (st.st_uid != geteuid())
        return fail(std::format("owned by uid {}, not {}", st.st_uid, geteuid()));
    if ((st.st_mode & 077) != 0 && fchmod(fd, 0600) != 0)
        return fail(std::format("chmod 0600: {}", strerror(errno)));

    return fd;
}
