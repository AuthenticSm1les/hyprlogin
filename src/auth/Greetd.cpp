#include "Greetd.hpp"

#include "../config/ConfigManager.hpp"
#include "../core/hyprlock.hpp"
#include "../helpers/Log.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstring>
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

static const char* responseTypeName(CGreetd::EResponseType type) {
    switch (type) {
        case CGreetd::EResponseType::SUCCESS: return "success";
        case CGreetd::EResponseType::ERROR: return "error";
        case CGreetd::EResponseType::AUTH_MESSAGE: return "auth_message";
        case CGreetd::EResponseType::INVALID:
        default: return "invalid";
    }
}

struct SGreetdFailResetPayload {
    std::string prompt;
    bool        secretInput = false;
    std::string username;
    std::string inputBuffer;
    std::string failText;
};

struct SGreetdPromptPayload {
    std::string prompt;
    bool        secretInput = false;
};

static void applyGreetdFailResetOnMainThread(ASP<CTimer> self, void* data) {
    std::unique_ptr<SGreetdFailResetPayload> payload{static_cast<SGreetdFailResetPayload*>(data)};
    if (!payload)
        return;

    g_pHyprlock->setGreeterUIState(payload->prompt, payload->secretInput, payload->username);
    g_pHyprlock->setInputBuffer(payload->inputBuffer);
    g_pAuth->enqueueFail(payload->failText, AUTH_IMPL_GREETD);
}

static void applyGreetdPromptOnMainThread(ASP<CTimer> self, void* data) {
    std::unique_ptr<SGreetdPromptPayload> payload{static_cast<SGreetdPromptPayload*>(data)};
    if (!payload)
        return;

    g_pHyprlock->setGreeterPrompt(payload->prompt, payload->secretInput);
}

void CGreetd::init() {
    const auto* sockPath = getenv("GREETD_SOCK");
    static const auto DEFAULTUSER = g_pConfigManager->getValue<Hyprlang::STRING>("sessions:default_user");
    if (!sockPath || std::string_view(sockPath).empty()) {
        setUnavailable("Run hyprlogin from greetd");
        Log::logger->log(Log::ERR, "GREETD_SOCK not set");
        return;
    }

    Log::logger->debug("init: GREETD_SOCK available at '{}'", sockPath);

    if (const std::string_view defaultUser = *DEFAULTUSER; !defaultUser.empty()) {
        m_username = std::string{defaultUser};
        g_pHyprlock->setTargetUsername(m_username);
        g_pHyprlock->setInputBuffer("");
        Log::logger->debug("init: default user '{}' selected, starting at password prompt", m_username);
        setPrompt(std::format("Password for {}", m_username), true);
        return;
    }

    Log::logger->debug("init: starting at username prompt");
    setPrompt("Enter username", false);
}

void CGreetd::handleInput(const std::string& input) {
    if (m_worker.joinable())
        m_worker.join();

    if (g_pHyprlock->getTargetUsername().empty()) {
        m_username = input;
        g_pHyprlock->setTargetUsername(input);
        Log::logger->debug("handleInput: captured username '{}'", input);
        setPrompt(std::format("Password for {}", input), true);
        g_pHyprlock->setInputBuffer("");
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
    Log::logger->debug("handleInput: starting auth transaction for user '{}'", g_pHyprlock->getTargetUsername());
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
        Log::logger->debug("runConversationThread: exception caught");
        Log::logger->log(Log::ERR, "Greetd worker thread exception: {}", e.what());
    } catch (...) {
        Log::logger->debug("runConversationThread: unknown exception caught");
        Log::logger->log(Log::ERR, "Greetd worker thread unknown exception");
    }
}

void CGreetd::runConversation(const std::string& input) {
    const auto finishConversation = [this]() {
        if (m_fd >= 0) {
            Log::logger->debug("runConversation: closing fd {}", m_fd);
            close(m_fd);
            m_fd = -1;
        }
    };

    Log::logger->debug("runConversation: worker thread started");

    if (!connectToServer()) {
        failAndReset("Unable to communicate with greetd", false);
        return;
    }

    const auto USERNAME = g_pHyprlock->getTargetUsername();
    Log::logger->debug("runConversation: create_session for '{}'", USERNAME);
    auto response = createSession(USERNAME);
    Log::logger->debug("create_session: type={} auth_type={} error_type='{}' description='{}' auth_message='{}'", responseTypeName(response.type),
                       (int)response.authType, response.errorType, response.description, response.authMessage);

    if (response.type == EResponseType::INVALID) {
        finishConversation();
        failAndReset("Unable to communicate with greetd", false);
        return;
    }

    if (response.type == EResponseType::SUCCESS) {
        handleResponse(response);
        finishConversation();
        return;
    }

    if (response.type == EResponseType::ERROR) {
        const auto failText = response.description.empty() ? "Authentication failed" : response.description;
        cancelSession();
        finishConversation();
        failAndReset(failText, false, true, isCooldownMessage(failText));
        return;
    }

    if (response.type != EResponseType::AUTH_MESSAGE) {
        finishConversation();
        failAndReset("Authentication failed", false, true);
        return;
    }

    switch (response.authType) {
        case EAuthMessageType::VISIBLE:
        case EAuthMessageType::SECRET:
            Log::logger->debug("runConversation: post_auth_message_response for '{}' with {} input", USERNAME,
                                       response.authType == EAuthMessageType::SECRET ? "secret" : "visible");
            response = postResponse(input);
            Log::logger->debug("post_auth_message_response: type={} auth_type={} error_type='{}' description='{}' auth_message='{}'", responseTypeName(response.type),
                               (int)response.authType, response.errorType, response.description, response.authMessage);
            break;
        case EAuthMessageType::INFO:
        case EAuthMessageType::ERROR: {
            const auto failText = response.authMessage.empty() ? "Authentication failed" : response.authMessage;
            cancelSession();
            finishConversation();
            failAndReset(failText, false, true, isCooldownMessage(failText));
            return;
        }
        case EAuthMessageType::INVALID:
        default:
            cancelSession();
            finishConversation();
            failAndReset("Authentication failed", false, true);
            return;
    }

    if (response.type == EResponseType::SUCCESS) {
        Log::logger->debug("runConversation: SUCCESS response, calling handleResponse");
        handleResponse(response);
        Log::logger->debug("runConversation: handleResponse returned, finishing conversation");
        finishConversation();
        Log::logger->debug("runConversation: worker thread exiting normally");
        return;
    }

    const auto failText = response.type == EResponseType::AUTH_MESSAGE && !response.authMessage.empty() ? response.authMessage :
                          (!response.description.empty() ? response.description : "Authentication failed");
    cancelSession();
    finishConversation();
    failAndReset(failText, false, true, isCooldownMessage(failText));
}

void CGreetd::handleResponse(const SResponse& response) {
    Log::logger->debug("handleResponse: type={} auth_type={} error_type='{}' description='{}' auth_message='{}'", responseTypeName(response.type),
                       (int)response.authType, response.errorType, response.description, response.authMessage);
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

            dispatchPromptToMainThread("Starting session", true);
            Log::logger->debug("handleResponse: start_session cmd='{}' env_count={}", SESSION, g_pHyprlock->getSelectedSessionEnv().size());
            const auto START = startSession(SESSION, g_pHyprlock->getSelectedSessionEnv());
            Log::logger->debug("start_session: type={} auth_type={} error_type='{}' description='{}' auth_message='{}'", responseTypeName(START.type),
                               (int)START.authType, START.errorType, START.description, START.authMessage);
            if (START.type != EResponseType::SUCCESS) {
                failAndReset(START.description.empty() ? "Failed to start session" : START.description, true);
                return;
            }

            g_pAuth->enqueueUnlock();
            Log::logger->debug("handleResponse: enqueueUnlock called, worker thread exiting normally");
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

    Log::logger->debug("failAndReset: fail='{}' reprompt_username={} effective_reprompt_username={} cooldown={} preserved_user='{}' default_user='{}'",
                                normalizedFailText, repromptUsername, effectiveRepromptUsername, cooldown, preservedUser, defaultUser);

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
        dispatchFailResetToMainThread(std::format("{} Enter username.", normalizedFailText), false, "", defaultUser, normalizedFailText);
    } else {
        const std::string newPrompt = cooldown ? std::format("Authentication cooldown for {}", preservedUser) : std::format("Password for {}", preservedUser);
        dispatchFailResetToMainThread(newPrompt, true, preservedUser, "", normalizedFailText);
    }
}

void CGreetd::dispatchFailResetToMainThread(std::string prompt, bool secretInput, std::string username, std::string inputBuffer, std::string failText) {
    auto* payload = new SGreetdFailResetPayload{
        .prompt      = std::move(prompt),
        .secretInput = secretInput,
        .username    = std::move(username),
        .inputBuffer = std::move(inputBuffer),
        .failText    = std::move(failText),
    };

    Log::logger->debug("dispatchFailResetToMainThread: prompt='{}' secret_input={} username='{}' input_len={} fail='{}'", payload->prompt,
                                payload->secretInput, payload->username, payload->inputBuffer.size(), payload->failText);

    g_pHyprlock->addTimer(std::chrono::milliseconds(0), applyGreetdFailResetOnMainThread, payload);
}

void CGreetd::dispatchPromptToMainThread(std::string prompt, bool secretInput) {
    auto* payload = new SGreetdPromptPayload{
        .prompt      = std::move(prompt),
        .secretInput = secretInput,
    };

    Log::logger->debug("dispatchPromptToMainThread: prompt='{}' secret_input={}", payload->prompt, payload->secretInput);
    g_pHyprlock->addTimer(std::chrono::milliseconds(0), applyGreetdPromptOnMainThread, payload);
}

bool CGreetd::connectToServer() {
    if (m_fd >= 0) {
        Log::logger->debug("connectToServer: reusing fd {}", m_fd);
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
        Log::logger->debug("connectToServer: socket() failed errno={} ({})", errno, strerror(errno));
        return false;
    }

    sockaddr_un addr = {};
    addr.sun_family  = AF_UNIX;
    std::strncpy(addr.sun_path, sockPath, sizeof(addr.sun_path) - 1);

    if (::connect(m_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
        Log::logger->log(Log::ERR, "Unable to connect to greetd socket: {}", strerror(errno));
        Log::logger->debug("connectToServer: connect() failed on fd {} errno={} ({})", m_fd, errno, strerror(errno));
        close(m_fd);
        m_fd = -1;
        return false;
    }

    Log::logger->debug("connectToServer: connected to GREETD_SOCK on fd {}", m_fd);

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
        Log::logger->debug("cancelSession: skipping, fd already closed");
        return {};
    }
    return roundtrip(R"({"type":"cancel_session"})");
}

CGreetd::SResponse CGreetd::roundtrip(const std::string& payload) {
    if (m_fd < 0 && !connectToServer())
        return {};

    Log::logger->debug("roundtrip: request on fd {} payload_bytes={}", m_fd, payload.size());
    Log::logger->debug("roundtrip: raw request: {}", payload);

    const uint32_t payloadLen = payload.size();
    const auto     lenWrite   = write(m_fd, &payloadLen, sizeof(payloadLen));
    if (lenWrite != static_cast<ssize_t>(sizeof(payloadLen))) {
        Log::logger->debug("roundtrip: failed writing length on fd {} wrote={} errno={} ({})", m_fd, lenWrite, errno, strerror(errno));
        return {};
    }

    const auto payloadWrite = write(m_fd, payload.data(), payload.size());
    if (payloadWrite != static_cast<ssize_t>(payload.size())) {
        Log::logger->debug("roundtrip: failed writing payload on fd {} wrote={} expected={} errno={} ({})", m_fd, payloadWrite, payload.size(), errno,
                                   strerror(errno));
        return {};
    }

    uint32_t responseLen = 0;
    const auto lenRead   = read(m_fd, &responseLen, sizeof(responseLen));
    if (lenRead != static_cast<ssize_t>(sizeof(responseLen))) {
        Log::logger->debug("roundtrip: failed reading length on fd {} read={} errno={} ({})", m_fd, lenRead, errno, strerror(errno));
        return {};
    }

    Log::logger->debug("roundtrip: response length {} bytes on fd {}", responseLen, m_fd);

    std::string response(responseLen, '\0');
    const auto responseRead = read(m_fd, response.data(), response.size());
    if (responseRead != static_cast<ssize_t>(response.size())) {
        Log::logger->debug("roundtrip: failed reading payload on fd {} read={} expected={} errno={} ({})", m_fd, responseRead, response.size(), errno,
                                   strerror(errno));
        return {};
    }

    Log::logger->debug("roundtrip: raw response: {}", response);
    Log::logger->debug("roundtrip: complete response on fd {}", m_fd);
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
