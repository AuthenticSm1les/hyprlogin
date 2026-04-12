#include "Greetd.hpp"

#include "../config/ConfigManager.hpp"
#include "../core/hyprlock.hpp"
#include "../helpers/Log.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <fstream>
#include <format>
#include <hyprlang.hpp>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

CGreetd::~CGreetd() {
    terminate();
}

eAuthImplementations CGreetd::getImplType() {
    return AUTH_IMPL_GREETD;
}

static bool greetdDebugMode() {
    static const auto DEBUGMODE = g_pConfigManager->getValue<Hyprlang::INT>("general:debug_mode");
    return *DEBUGMODE != 0;
}

static std::string greetdDebugLogPath() {
    static const auto DEBUGLOGPATH = g_pConfigManager->getValue<Hyprlang::STRING>("general:debug_log_path");
    return std::string{std::string_view{*DEBUGLOGPATH}};
}

static void appendGreetdDebugFile(const std::string& message) {
    if (!greetdDebugMode())
        return;

    const auto path = greetdDebugLogPath();
    if (path.empty())
        return;

    std::ofstream out(path, std::ios::app);
    if (!out.is_open())
        return;

    out << message << '\n';
}

static void logGreetdDebug(const std::string& message) {
    if (!greetdDebugMode())
        return;

    Log::logger->log(Log::INFO, "[greetd-debug] {}", message);
    appendGreetdDebugFile(message);
}

static const char* responseTypeName(CGreetd::EResponseType type) {
    switch (type) {
        case CGreetd::EResponseType::SUCCESS: return "success";
        case CGreetd::EResponseType::ERROR: return "error";
        case CGreetd::EResponseType::AUTH_MESSAGE: return "auth_message";
        case CGreetd::EResponseType::INVALID:
        default: return "invalid";
    }
}

static const char* authMessageTypeName(CGreetd::EAuthMessageType type) {
    switch (type) {
        case CGreetd::EAuthMessageType::VISIBLE: return "visible";
        case CGreetd::EAuthMessageType::SECRET: return "secret";
        case CGreetd::EAuthMessageType::INFO: return "info";
        case CGreetd::EAuthMessageType::ERROR: return "error";
        case CGreetd::EAuthMessageType::INVALID:
        default: return "invalid";
    }
}

static void logGreetdResponse(const char* stage, const CGreetd::SResponse& response) {
    if (!greetdDebugMode())
        return;

    Log::logger->log(Log::INFO, "[greetd-debug] {}: type={} auth_type={} error_type='{}' description='{}' auth_message='{}'", stage, responseTypeName(response.type),
                     authMessageTypeName(response.authType), response.errorType, response.description, response.authMessage);
}

void CGreetd::init() {
    const auto* sockPath = getenv("GREETD_SOCK");
    static const auto DEFAULTUSER = g_pConfigManager->getValue<Hyprlang::STRING>("sessions:default_user");
    if (!sockPath || std::string_view(sockPath).empty()) {
        setUnavailable("Run hyprlogin from greetd");
        Log::logger->log(Log::ERR, "GREETD_SOCK not set");
        return;
    }

    logGreetdDebug(std::format("init: GREETD_SOCK available at '{}'", sockPath));

    if (const std::string_view defaultUser = *DEFAULTUSER; !defaultUser.empty()) {
        m_username = std::string{defaultUser};
        g_pHyprlock->setTargetUsername(m_username);
        g_pHyprlock->setInputBuffer("");
        logGreetdDebug(std::format("init: default user '{}' selected, starting at password prompt", m_username));
        setPrompt(std::format("Password for {}", m_username), true);
        return;
    }

    logGreetdDebug("init: starting at username prompt");
    setPrompt("Enter username", false);
}

void CGreetd::handleInput(const std::string& input) {
    if (m_worker.joinable())
        m_worker.join();

    if (g_pHyprlock->getTargetUsername().empty()) {
        m_username = input;
        g_pHyprlock->setTargetUsername(input);
        logGreetdDebug(std::format("handleInput: captured username '{}'", input));
        setPrompt(std::format("Password for {}", input), true);
        g_pHyprlock->setInputBuffer("");
        g_pHyprlock->renderAllOutputs();
        return;
    }

    {
        std::lock_guard<std::mutex> guard(m_stateMutex);
        if (!m_available)
            return;

        if (m_waitingForServer)
            return;

        m_waitingForServer = true;
        m_waitingForUser   = false;
    }

    g_pHyprlock->setGreeterPrompt("Validating...", true);
    g_pHyprlock->renderAllOutputs();
    logGreetdDebug(std::format("handleInput: starting auth transaction for user '{}'", g_pHyprlock->getTargetUsername()));
    m_worker = std::thread([this, input]() {
        this->runConversationThread(input);
    });
}

bool CGreetd::checkWaiting() {
    std::lock_guard<std::mutex> guard(m_stateMutex);
    return m_waitingForServer;
}

std::optional<std::string> CGreetd::getLastFailText() {
    std::lock_guard<std::mutex> guard(m_stateMutex);
    return m_lastFail.empty() ? std::nullopt : std::optional(m_lastFail);
}

std::optional<std::string> CGreetd::getLastPrompt() {
    std::lock_guard<std::mutex> guard(m_stateMutex);
    return m_lastPrompt.empty() ? std::nullopt : std::optional(m_lastPrompt);
}

void CGreetd::terminate() {
    {
        std::lock_guard<std::mutex> guard(m_stateMutex);
        m_waitingForServer = false;
    }

    if (m_worker.joinable())
        m_worker.join();

    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
    }
}

void CGreetd::setPrompt(const std::string& prompt, bool secret) {
    std::string resolvedPrompt = prompt;
    if (resolvedPrompt.empty())
        resolvedPrompt = secret ? "Password" : "Username";

    if (!secret && (resolvedPrompt == "Username" || resolvedPrompt == "login:"))
        resolvedPrompt = "Enter username";

    if (secret && (resolvedPrompt == "Password" || resolvedPrompt == "Password:" || resolvedPrompt == "Password for" || resolvedPrompt == "Password: ")) {
        if (!m_username.empty())
            resolvedPrompt = std::format("Password for {}", m_username);
        else
            resolvedPrompt = "Enter password";
    }

    {
        std::lock_guard<std::mutex> guard(m_stateMutex);
        m_available        = true;
        m_lastPrompt       = resolvedPrompt;
        m_waitingForSecret = secret;
        m_waitingForUser   = true;
        m_waitingForServer = false;
    }

    g_pHyprlock->setGreeterPrompt(resolvedPrompt, secret);
}

void CGreetd::setUnavailable(const std::string& reason) {
    {
        std::lock_guard<std::mutex> guard(m_stateMutex);
        m_available        = false;
        m_lastFail         = reason;
        m_lastPrompt       = reason;
        m_waitingForSecret = false;
        m_waitingForServer = false;
        m_waitingForUser   = false;
        m_username.clear();
    }

    g_pHyprlock->setGreeterPrompt(reason, false);
    g_pHyprlock->clearTargetUsername();
}

void CGreetd::runConversationThread(const std::string& input) {
    try {
        runConversation(input);
    } catch (const std::exception& e) {
        logGreetdDebug("runConversationThread: exception caught");
        Log::logger->log(Log::ERR, "Greetd worker thread exception: {}", e.what());
    } catch (...) {
        logGreetdDebug("runConversationThread: unknown exception caught");
        Log::logger->log(Log::ERR, "Greetd worker thread unknown exception");
    }
}

void CGreetd::runConversation(const std::string& input) {
    const auto finishConversation = [this]() {
        if (m_fd >= 0) {
            logGreetdDebug(std::format("runConversation: closing fd {}", m_fd));
            close(m_fd);
            m_fd = -1;
        }
    };

    logGreetdDebug("runConversation: worker thread started");

    if (!connectToServer()) {
        failAndReset("Unable to communicate with greetd", false);
        g_pHyprlock->renderAllOutputs();
        return;
    }

    const auto USERNAME = g_pHyprlock->getTargetUsername();
    logGreetdDebug(std::format("runConversation: create_session for '{}'", USERNAME));
    auto       response = createSession(USERNAME);
    logGreetdResponse("create_session", response);

    if (response.type == EResponseType::INVALID) {
        finishConversation();
        failAndReset("Unable to communicate with greetd", false);
        g_pHyprlock->renderAllOutputs();
        return;
    }

    if (response.type == EResponseType::SUCCESS) {
        handleResponse(response);
        finishConversation();
        g_pHyprlock->renderAllOutputs();
        return;
    }

    if (response.type == EResponseType::ERROR) {
        const auto failText = response.description.empty() ? "Authentication failed" : response.description;
        cancelSession();
        finishConversation();
        failAndReset(failText, false, true, isCooldownMessage(failText));
        g_pHyprlock->renderAllOutputs();
        return;
    }

    if (response.type != EResponseType::AUTH_MESSAGE) {
        finishConversation();
        failAndReset("Authentication failed", false, true);
        g_pHyprlock->renderAllOutputs();
        return;
    }

    switch (response.authType) {
        case EAuthMessageType::VISIBLE:
        case EAuthMessageType::SECRET:
            logGreetdDebug(std::format("runConversation: post_auth_message_response for '{}' with {} input", USERNAME,
                                       response.authType == EAuthMessageType::SECRET ? "secret" : "visible"));
            response = postResponse(input);
            logGreetdResponse("post_auth_message_response", response);
            break;
        case EAuthMessageType::INFO:
        case EAuthMessageType::ERROR: {
            const auto failText = response.authMessage.empty() ? "Authentication failed" : response.authMessage;
            cancelSession();
            finishConversation();
            failAndReset(failText, false, true, isCooldownMessage(failText));
            g_pHyprlock->renderAllOutputs();
            return;
        }
        case EAuthMessageType::INVALID:
        default:
            cancelSession();
            finishConversation();
            failAndReset("Authentication failed", false, true);
            g_pHyprlock->renderAllOutputs();
            return;
    }

    if (response.type == EResponseType::SUCCESS) {
        logGreetdDebug("runConversation: SUCCESS response, calling handleResponse");
        handleResponse(response);
        logGreetdDebug("runConversation: handleResponse returned, finishing conversation");
        finishConversation();
        logGreetdDebug("runConversation: calling renderAllOutputs before thread exit");
        g_pHyprlock->renderAllOutputs();
        logGreetdDebug("runConversation: worker thread exiting normally");
        return;
    }

    const auto failText = response.type == EResponseType::AUTH_MESSAGE && !response.authMessage.empty() ? response.authMessage :
                          (!response.description.empty() ? response.description : "Authentication failed");
    cancelSession();
    finishConversation();
    failAndReset(failText, false, true, isCooldownMessage(failText));
    g_pHyprlock->renderAllOutputs();
}

void CGreetd::handleResponse(const SResponse& response) {
    logGreetdResponse("handleResponse", response);
    switch (response.type) {
        case EResponseType::SUCCESS: {
            const auto SESSION = g_pHyprlock->getSelectedSessionCommand();
            if (SESSION.empty()) {
                failAndReset("No session selected", true);
                return;
            }

            {
                std::lock_guard<std::mutex> guard(m_stateMutex);
                m_lastPrompt = "Starting session";
            }

            g_pHyprlock->setGreeterPrompt("Starting session", true);
            logGreetdDebug(std::format("handleResponse: start_session cmd='{}' env_count={}", SESSION, g_pHyprlock->getSelectedSessionEnv().size()));
            const auto START = startSession(SESSION, g_pHyprlock->getSelectedSessionEnv());
            logGreetdResponse("start_session", START);
            if (START.type != EResponseType::SUCCESS) {
                failAndReset(START.description.empty() ? "Failed to start session" : START.description, true);
                return;
            }

            g_pAuth->enqueueUnlock();
            logGreetdDebug("handleResponse: enqueueUnlock called, worker thread exiting normally");
            return;
        }
        case EResponseType::AUTH_MESSAGE: {
            const auto failText = response.authMessage.empty() ? "Authentication failed" : response.authMessage;
            cancelSession();
            failAndReset(failText, false, true, isCooldownMessage(failText));
            return;
        }
        case EResponseType::ERROR: {
            const auto failText         = response.description.empty() ? "greetd request failed" : response.description;
            const bool repromptUsername = !wasWaitingForSecret() || shouldRepromptUsername(failText);
            failAndReset(failText, true, repromptUsername, isCooldownMessage(failText));
            return;
        }
        case EResponseType::INVALID:
        default: failAndReset("Unable to communicate with greetd", false); return;
    }
}

bool CGreetd::wasWaitingForSecret() {
    std::lock_guard<std::mutex> guard(m_stateMutex);
    return m_waitingForSecret;
}

bool CGreetd::shouldRepromptUsername(const std::string& failText) const {
    std::string text = failText;
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return text.contains("user unknown") || text.contains("unknown user") || text.contains("username") || text.contains("not exist");
}

bool CGreetd::isCooldownMessage(const std::string& failText) const {
    std::string text = failText;
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return text.contains("left to unlock") || text.contains("locked") || text.contains("cooldown") || text.contains("wait");
}

std::string CGreetd::normalizeFailText(const std::string& failText) const {
    if (isCooldownMessage(failText))
        return failText.empty() ? "Authentication cooldown active. Please wait before retrying." : failText;

    return "Invalid username or password. Try again.";
}

void CGreetd::failAndReset(const std::string& failText, bool cancelSessionRequest, bool repromptUsername, bool cooldown) {
    static const auto DEFAULTUSER = g_pConfigManager->getValue<Hyprlang::STRING>("sessions:default_user");
    const auto        normalizedFailText = normalizeFailText(failText);
    const auto        defaultUser        = std::string{std::string_view{*DEFAULTUSER}};
    const auto        preservedUser      = m_username.empty() ? defaultUser : m_username;
    const bool        effectiveRepromptUsername = repromptUsername && defaultUser.empty();

    // Unknown-user failures can happen before greetd creates a real auth session.
    // In that case, cancel_session can block the reprompt path instead of helping it.
    if (cancelSessionRequest && !effectiveRepromptUsername)
        cancelSession();

    logGreetdDebug(std::format("failAndReset: fail='{}' reprompt_username={} effective_reprompt_username={} cooldown={} preserved_user='{}' default_user='{}'",
                               normalizedFailText, repromptUsername, effectiveRepromptUsername, cooldown, preservedUser, defaultUser));

    {
        std::lock_guard<std::mutex> guard(m_stateMutex);
        m_lastFail         = normalizedFailText;
        m_waitingForServer = false;
        m_waitingForUser   = true;
        m_waitingForSecret = !effectiveRepromptUsername;
        m_lastPrompt       = effectiveRepromptUsername ? std::format("{} Enter username.", normalizedFailText) :
                                               (cooldown ? std::format("Authentication cooldown for {}", preservedUser) : std::format("Password for {}", preservedUser));
        if (effectiveRepromptUsername)
            m_username.clear();
    }

    if (effectiveRepromptUsername) {
        g_pHyprlock->setGreeterUIState(std::format("{} Enter username.", normalizedFailText), false, "");
        g_pHyprlock->setInputBuffer(defaultUser);
    } else {
        const std::string newPrompt = cooldown ? std::format("Authentication cooldown for {}", preservedUser) : std::format("Password for {}", preservedUser);
        g_pHyprlock->setGreeterUIState(newPrompt, true, preservedUser);
        g_pHyprlock->setInputBuffer("");
    }

    g_pAuth->enqueueFail(normalizedFailText, AUTH_IMPL_GREETD);
}

bool CGreetd::connectToServer() {
    if (m_fd >= 0) {
        logGreetdDebug(std::format("connectToServer: reusing fd {}", m_fd));
        return true;
    }

    const auto* sockPath = getenv("GREETD_SOCK");
    if (!sockPath || std::string_view(sockPath).empty()) {
        Log::logger->log(Log::ERR, "GREETD_SOCK not set");
        return false;
    }

    m_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_fd < 0) {
        Log::logger->log(Log::ERR, "Unable to create greetd socket");
        logGreetdDebug(std::format("connectToServer: socket() failed errno={} ({})", errno, strerror(errno)));
        return false;
    }

    sockaddr_un addr = {};
    addr.sun_family  = AF_UNIX;
    std::strncpy(addr.sun_path, sockPath, sizeof(addr.sun_path) - 1);

    if (::connect(m_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
        Log::logger->log(Log::ERR, "Unable to connect to greetd socket: {}", strerror(errno));
        logGreetdDebug(std::format("connectToServer: connect() failed on fd {} errno={} ({})", m_fd, errno, strerror(errno)));
        close(m_fd);
        m_fd = -1;
        return false;
    }

    logGreetdDebug(std::format("connectToServer: connected to GREETD_SOCK on fd {}", m_fd));

    return true;
}

CGreetd::SResponse CGreetd::createSession(const std::string& username) {
    return roundtrip(std::format(R"({{"type":"create_session","username":"{}"}})", escapeJson(username)));
}

CGreetd::SResponse CGreetd::postResponse(const std::string& response) {
    return roundtrip(std::format(R"({{"type":"post_auth_message_response","response":"{}"}})", escapeJson(response)));
}

CGreetd::SResponse CGreetd::startSession(const std::string& command, const std::vector<std::string>& env) {
    std::string envJson = "[";
    for (size_t i = 0; i < env.size(); ++i) {
        if (i != 0)
            envJson += ",";
        envJson += std::format("\"{}\"", escapeJson(env[i]));
    }
    envJson += "]";

    return roundtrip(std::format(R"({{"type":"start_session","cmd":["{}"],"env":{}}})", escapeJson(command), envJson));
}

CGreetd::SResponse CGreetd::cancelSession() {
    // Don't attempt to cancel if the socket is already closed.
    // greetd often closes the connection on auth failure before we get here.
    if (m_fd < 0) {
        logGreetdDebug("cancelSession: skipping, fd already closed");
        return {};
    }
    return roundtrip(R"({"type":"cancel_session"})");
}

CGreetd::SResponse CGreetd::roundtrip(const std::string& payload) {
    if (m_fd < 0 && !connectToServer())
        return {};

    logGreetdDebug(std::format("roundtrip: request on fd {} payload_bytes={}", m_fd, payload.size()));
    if (greetdDebugMode())
        Log::logger->log(Log::INFO, "[greetd-debug] request: {}", payload);

    const uint32_t payloadLen = payload.size();
    const auto     lenWrite   = write(m_fd, &payloadLen, sizeof(payloadLen));
    if (lenWrite != static_cast<ssize_t>(sizeof(payloadLen))) {
        logGreetdDebug(std::format("roundtrip: failed writing length on fd {} wrote={} errno={} ({})", m_fd, lenWrite, errno, strerror(errno)));
        return {};
    }

    const auto payloadWrite = write(m_fd, payload.data(), payload.size());
    if (payloadWrite != static_cast<ssize_t>(payload.size())) {
        logGreetdDebug(std::format("roundtrip: failed writing payload on fd {} wrote={} expected={} errno={} ({})", m_fd, payloadWrite, payload.size(), errno,
                                   strerror(errno)));
        return {};
    }

    uint32_t responseLen = 0;
    const auto lenRead   = read(m_fd, &responseLen, sizeof(responseLen));
    if (lenRead != static_cast<ssize_t>(sizeof(responseLen))) {
        logGreetdDebug(std::format("roundtrip: failed reading length on fd {} read={} errno={} ({})", m_fd, lenRead, errno, strerror(errno)));
        return {};
    }

    logGreetdDebug(std::format("roundtrip: response length {} bytes on fd {}", responseLen, m_fd));

    std::string response(responseLen, '\0');
    const auto responseRead = read(m_fd, response.data(), response.size());
    if (responseRead != static_cast<ssize_t>(response.size())) {
        logGreetdDebug(std::format("roundtrip: failed reading payload on fd {} read={} expected={} errno={} ({})", m_fd, responseRead, response.size(), errno,
                                   strerror(errno)));
        return {};
    }

    if (greetdDebugMode())
        Log::logger->log(Log::INFO, "[greetd-debug] response: {}", response);
    logGreetdDebug(std::format("roundtrip: complete response on fd {}", m_fd));
    return parseResponse(response);
}

std::string CGreetd::escapeJson(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());

    for (const auto ch : value) {
        switch (ch) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += ch; break;
        }
    }

    return escaped;
}

std::string CGreetd::extractJsonString(const std::string& json, const std::string& key) {
    const auto keyPattern = std::format("\"{}\"", key);
    const auto keyPos     = json.find(keyPattern);
    if (keyPos == std::string::npos)
        return "";

    const auto colonPos = json.find(':', keyPos + keyPattern.size());
    if (colonPos == std::string::npos)
        return "";

    const auto firstQuote = json.find('"', colonPos + 1);
    if (firstQuote == std::string::npos)
        return "";

    std::string value;
    bool        escaped = false;
    for (size_t i = firstQuote + 1; i < json.size(); ++i) {
        const auto ch = json[i];
        if (escaped) {
            switch (ch) {
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                default: value.push_back(ch); break;
            }
            escaped = false;
            continue;
        }

        if (ch == '\\') {
            escaped = true;
            continue;
        }

        if (ch == '"')
            break;

        value.push_back(ch);
    }

    return value;
}

CGreetd::SResponse CGreetd::parseResponse(const std::string& json) {
    SResponse response;

    const auto type = extractJsonString(json, "type");
    if (type == "success")
        response.type = EResponseType::SUCCESS;
    else if (type == "error")
        response.type = EResponseType::ERROR;
    else if (type == "auth_message")
        response.type = EResponseType::AUTH_MESSAGE;
    else
        return response;

    response.errorType   = extractJsonString(json, "error_type");
    response.description = extractJsonString(json, "description");
    response.authMessage = extractJsonString(json, "auth_message");

    const auto authType = extractJsonString(json, "auth_message_type");
    if (authType == "visible")
        response.authType = EAuthMessageType::VISIBLE;
    else if (authType == "secret")
        response.authType = EAuthMessageType::SECRET;
    else if (authType == "info")
        response.authType = EAuthMessageType::INFO;
    else if (authType == "error")
        response.authType = EAuthMessageType::ERROR;

    return response;
}
