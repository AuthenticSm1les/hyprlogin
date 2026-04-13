#pragma once

#include "Auth.hpp"

#include <mutex>
#include <thread>

class CGreetd : public IAuthImplementation {
  public:
    CGreetd() = default;
    ~CGreetd() override;

    eAuthImplementations       getImplType() override;
    void                       init() override;
    void                       handleInput(const std::string& input) override;
    bool                       checkWaiting() override;
    std::optional<std::string> getLastFailText() override;
    std::optional<std::string> getLastPrompt() override;
    void                       terminate() override;
    enum class EResponseType {
        INVALID = 0,
        SUCCESS,
        ERROR,
        AUTH_MESSAGE,
    };

    enum class EAuthMessageType {
        INVALID = 0,
        VISIBLE,
        SECRET,
        INFO,
        ERROR,
    };

    struct SResponse {
        EResponseType    type        = EResponseType::INVALID;
        EAuthMessageType authType    = EAuthMessageType::INVALID;
        std::string      authMessage = "";
        std::string      errorType   = "";
        std::string      description = "";
    };

  private:

    void      setPrompt(const std::string& prompt, bool secret);
    void      setUnavailable(const std::string& reason);
    void      runConversation(const std::string& input);
    void      runConversationThread(const std::string& input);
    void      handleResponse(const SResponse& response);
    void      failAndReset(const std::string& failText, bool cancelSession, bool repromptUsername = true, bool cooldown = false);
    void      dispatchPromptToMainThread(std::string prompt, bool secretInput);
    void      dispatchFailResetToMainThread(std::string prompt, bool secretInput, std::string username, std::string inputBuffer, std::string failText);
    bool      wasWaitingForSecret();
    bool      shouldRepromptUsername(const std::string& failText) const;
    bool      isCooldownMessage(const std::string& failText) const;
    std::string normalizeFailText(const std::string& failText) const;

    bool      connectToServer();
    SResponse createSession(const std::string& username);
    SResponse postResponse(const std::string& response);
    SResponse startSession(const std::string& command, const std::vector<std::string>& env);
    SResponse cancelSession();
    SResponse roundtrip(const std::string& payload);

    static std::string escapeJson(const std::string& value);
    static std::string extractJsonString(const std::string& json, const std::string& key);
    static SResponse   parseResponse(const std::string& json);

    std::thread m_worker;
    std::mutex  m_stateMutex;
    bool        m_waitingForServer = false;
    bool        m_waitingForUser   = true;
    bool        m_waitingForSecret = false;
    bool        m_available        = true;
    std::string m_lastFail         = "";
    std::string m_lastPrompt       = "Username";
    std::string m_username         = "";
    int         m_fd               = -1;
};
