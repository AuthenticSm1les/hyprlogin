#pragma once
#include "../defines.hpp"

#include <format>
#include <print>
#include <chrono>
#include <unistd.h>
#include <hyprutils/cli/Logger.hpp>

#define RASSERT(expr, reason, ...)                                                                                                                                                 \
    if (!(expr)) {                                                                                                                                                                 \
        Log::logger->log(Log::CRIT, "\n==========================================================================================\nASSERTION FAILED! \n\n{}\n\nat: line {} in {}", \
                         std::format(reason, ##__VA_ARGS__), __LINE__,                                                                                                             \
                         ([]() constexpr -> std::string { return std::string(__FILE__).substr(std::string(__FILE__).find_last_of('/') + 1); })().c_str());                         \
        std::abort();                                                                                                                                                              \
    }

#define ASSERT(expr) RASSERT(expr, "?")

namespace Log {
    class CLogger {
      public:
        template <typename... Args>
        void log(Hyprutils::CLI::eLogLevel level, std::format_string<Args...> fmt, Args&&... args) {
            if (m_quiet && m_logFd < 0) // hyprlogin: --quiet still writes --log-file
                return;

            if (level == Hyprutils::CLI::LOG_TRACE && !m_verbose)
                return;

            const auto msg = std::vformat(fmt.get(), std::make_format_args(args...));
            writeLogFile(level, msg); // hyprlogin

            if (!m_quiet)
                m_logger.log(level, msg);
        }

        // hyprlogin: [debug] messages, enabled by --debug (see greeter/Logging.hpp)
        template <typename... Args>
        void debug(std::format_string<Args...> fmt, Args&&... args) {
            if (!m_debugMode)
                return;

            log(Hyprutils::CLI::LOG_DEBUG, "[debug] {}", std::vformat(fmt.get(), std::make_format_args(args...)));
        }

        // hyprlogin
        void setDebug() {
            m_debugMode = true;
            setVerbose();
        }

        // hyprlogin: takes ownership of an O_APPEND fd (see GreeterLogging::openLogFile).
        // Lines are written unbuffered so a crash or ASSERT abort doesn't lose them.
        void setLogFd(int fd) {
            if (m_logFd >= 0)
                close(m_logFd);
            m_logFd = fd;
        }

        void setVerbose() {
            m_verbose = true;
            m_logger.setLogLevel(Hyprutils::CLI::LOG_TRACE);
        }

        void setQuiet() {
            m_quiet = true;
        }

        bool verbose() {
            return m_verbose;
        }

      private:
        bool                    m_quiet   = false;
        bool                    m_verbose = false;

        // hyprlogin
        bool m_debugMode = false;
        int  m_logFd     = -1;

        void writeLogFile(Hyprutils::CLI::eLogLevel level, const std::string& msg) {
            if (m_logFd < 0)
                return;

            static constexpr const char* NAMES[] = {"TRACE", "DEBUG", "WARN", "ERR", "CRIT"};
            const auto                   now     = std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now());
            const char*                  name    = level <= Hyprutils::CLI::LOG_CRIT ? NAMES[level] : "?";

            // local time, to match journalctl; UTC if the tz database is unavailable
            static const std::chrono::time_zone* zone = []() -> const std::chrono::time_zone* {
                try {
                    return std::chrono::current_zone();
                } catch (...) { return nullptr; }
            }();

            const auto line = zone ? std::format("[{:%F %T}] {}: {}\n", std::chrono::zoned_time{zone, now}, name, msg) :
                                     std::format("[{:%F %T} UTC] {}: {}\n", now, name, msg);
            (void)!write(m_logFd, line.data(), line.size());
        }

        Hyprutils::CLI::CLogger m_logger;
    };

    inline UP<CLogger> logger = makeUnique<CLogger>();

    //
    inline constexpr const Hyprutils::CLI::eLogLevel WARN  = Hyprutils::CLI::LOG_WARN;
    inline constexpr const Hyprutils::CLI::eLogLevel ERR   = Hyprutils::CLI::LOG_ERR;
    inline constexpr const Hyprutils::CLI::eLogLevel CRIT  = Hyprutils::CLI::LOG_CRIT;
    inline constexpr const Hyprutils::CLI::eLogLevel INFO  = Hyprutils::CLI::LOG_DEBUG;
    inline constexpr const Hyprutils::CLI::eLogLevel TRACE = Hyprutils::CLI::LOG_TRACE;
};
