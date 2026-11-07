#include "Greeter.hpp"
#include "RememberCache.hpp"

#include "../config/ConfigManager.hpp"
#include "../core/Seat.hpp"
#include "../core/hyprlock.hpp"
#include "../helpers/Log.hpp"
#include "../helpers/MiscFunctions.hpp"

#include <algorithm>
#include <hyprutils/string/String.hpp>

using namespace Hyprutils::String;

void CGreeter::registerConfig(Hyprlang::CConfig& config) {
    config.addConfigValue("general:exit_command", Hyprlang::STRING{"hyprctl dispatch exit"});

    config.addConfigValue("sessions:wayland_path", Hyprlang::STRING{"/usr/share/wayland-sessions"});
    config.addConfigValue("sessions:x11_path", Hyprlang::STRING{"/usr/share/xsessions"});
    config.addConfigValue("sessions:default_user", Hyprlang::STRING{""});
    config.addConfigValue("sessions:default_session", Hyprlang::STRING{""});
    config.addConfigValue("sessions:remember_user", Hyprlang::INT{1});
    config.addConfigValue("sessions:remember_session", Hyprlang::INT{1});
    config.addConfigValue("sessions:remember_dir", Hyprlang::STRING{HYPRLOGIN_CACHE_DIR});

    config.addSpecialConfigValue("input-field", "placeholder_text_username", Hyprlang::STRING{"<i>Input Username</i>"});
    config.addSpecialConfigValue("label", "hide_when_empty", Hyprlang::INT{0});
}

void CGreeter::init() {
    static const auto WAYLANDPATH     = g_pConfigManager->getValue<Hyprlang::STRING>("sessions:wayland_path");
    static const auto X11PATH         = g_pConfigManager->getValue<Hyprlang::STRING>("sessions:x11_path");
    static const auto DEFAULTUSER     = g_pConfigManager->getValue<Hyprlang::STRING>("sessions:default_user");
    static const auto DEFAULTSESSION  = g_pConfigManager->getValue<Hyprlang::STRING>("sessions:default_session");
    static const auto REMEMBERUSER    = g_pConfigManager->getValue<Hyprlang::INT>("sessions:remember_user");
    static const auto REMEMBERSESSION = g_pConfigManager->getValue<Hyprlang::INT>("sessions:remember_session");
    static const auto REMEMBERDIR     = g_pConfigManager->getValue<Hyprlang::STRING>("sessions:remember_dir");

    m_state.sessions = Sessions::discover(*WAYLANDPATH, *X11PATH);

    // Remembered values win; the configured defaults are the first-boot
    // fallback and cover a remembered session that no longer exists.
    std::vector<std::string> sessionCandidates;
    if (*REMEMBERSESSION) {
        if (auto remembered = RememberCache::read(*REMEMBERDIR, RememberCache::SESSION_ENTRY); remembered.has_value())
            sessionCandidates.emplace_back(std::move(*remembered));
    }
    if (const std::string_view configured = *DEFAULTSESSION; !configured.empty())
        sessionCandidates.emplace_back(configured);

    for (const auto& candidate : sessionCandidates) {
        const auto it = std::ranges::find_if(m_state.sessions, [&candidate](const auto& session) { return Sessions::matches(session, candidate); });
        if (it != m_state.sessions.end()) {
            m_state.selectedSessionIndex = std::distance(m_state.sessions.begin(), it);
            break;
        }
    }

    m_state.defaultUser = std::string{std::string_view{*DEFAULTUSER}};
    if (*REMEMBERUSER) {
        if (auto remembered = RememberCache::read(*REMEMBERDIR, RememberCache::USER_ENTRY); remembered.has_value())
            m_state.defaultUser = std::move(*remembered);
    }

    Log::logger->debug("greeter: {} session(s), selected='{}', default user='{}'", m_state.sessions.size(), getSelectedSessionName(), m_state.defaultUser);
}

void CGreeter::refresh() {
    if (!g_pHyprlock)
        return;

    g_pHyprlock->enqueueForceUpdateTimers();
    g_pHyprlock->renderAllOutputs();
}

void CGreeter::setPrompt(const std::string& prompt, bool secretInput) {
    m_state.prompt      = prompt;
    m_state.secretInput = secretInput;
    Log::logger->debug("greeter: prompt='{}' secret_input={}", prompt, secretInput);
    refresh();
}

const std::string& CGreeter::getPrompt() {
    return m_state.prompt;
}

bool CGreeter::isSecretInput() {
    return m_state.secretInput;
}

void CGreeter::setTargetUsername(const std::string& username) {
    m_state.targetUsername = username;
    Log::logger->debug("greeter: target user='{}'", username);
    refresh();
}

void CGreeter::clearTargetUsername() {
    setTargetUsername("");
}

const std::string& CGreeter::getTargetUsername() {
    return m_state.targetUsername;
}

void CGreeter::setUIState(const std::string& prompt, bool secretInput, const std::string& username) {
    m_state.prompt         = prompt;
    m_state.secretInput    = secretInput;
    m_state.targetUsername = username;
    Log::logger->debug("greeter: prompt='{}' secret_input={} target user='{}'", prompt, secretInput, username);
    refresh();
}

void CGreeter::cycleSession(int delta) {
    if (m_state.sessions.empty())
        return;

    const auto count = static_cast<int>(m_state.sessions.size());
    m_state.selectedSessionIndex = ((static_cast<int>(m_state.selectedSessionIndex) + delta) % count + count) % count;
    Log::logger->debug("greeter: session='{}'", getSelectedSessionName());
    refresh();
}

std::string CGreeter::getSelectedSessionName() {
    if (m_state.sessions.empty())
        return "No sessions found";

    return m_state.sessions.at(m_state.selectedSessionIndex).name;
}

std::string CGreeter::getSelectedSessionCommand() {
    if (m_state.sessions.empty())
        return "";

    return m_state.sessions.at(m_state.selectedSessionIndex).command;
}

std::vector<std::string> CGreeter::getSelectedSessionEnv() {
    if (m_state.sessions.empty())
        return {};

    return Sessions::environment(m_state.sessions.at(m_state.selectedSessionIndex));
}

const std::string& CGreeter::getDefaultUser() {
    return m_state.defaultUser;
}

void CGreeter::rememberLogin(const std::string& username) {
    static const auto REMEMBERUSER    = g_pConfigManager->getValue<Hyprlang::INT>("sessions:remember_user");
    static const auto REMEMBERSESSION = g_pConfigManager->getValue<Hyprlang::INT>("sessions:remember_session");
    static const auto REMEMBERDIR     = g_pConfigManager->getValue<Hyprlang::STRING>("sessions:remember_dir");
    const std::string dir             = std::string{std::string_view{*REMEMBERDIR}};

    if (*REMEMBERUSER && !username.empty() && !RememberCache::write(dir, RememberCache::USER_ENTRY, username))
        Log::logger->log(Log::WARN, "Couldn't remember user in {} (is it writable by the greeter user?)", dir);

    if (*REMEMBERSESSION && !m_state.sessions.empty() &&
        !RememberCache::write(dir, RememberCache::SESSION_ENTRY, Sessions::id(m_state.sessions.at(m_state.selectedSessionIndex))))
        Log::logger->log(Log::WARN, "Couldn't remember session in {} (is it writable by the greeter user?)", dir);
}

bool CGreeter::handleKeySym(xkb_keysym_t sym) {
    // Escape on an empty password field goes back to username entry.
    if (sym == XKB_KEY_Escape && !m_state.targetUsername.empty() && m_state.secretInput && g_pHyprlock->getInputBuffer().empty()) {
        Log::logger->log(Log::INFO, "Returning to username input");
        setUIState("Enter username", false, "");
        return true;
    }

    if (sym == XKB_KEY_Tab || sym == XKB_KEY_ISO_Left_Tab || sym == XKB_KEY_Left || sym == XKB_KEY_Right) {
        const bool SHIFT = g_pSeatManager->m_pXKBState && xkb_state_mod_name_is_active(g_pSeatManager->m_pXKBState, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE) > 0;
        cycleSession((sym == XKB_KEY_Left || sym == XKB_KEY_ISO_Left_Tab || (sym == XKB_KEY_Tab && SHIFT)) ? -1 : 1);
        return true;
    }

    return false;
}

bool CGreeter::handleInternalCommand(const std::string& cmd) {
    if (cmd == "hyprlogin:session_next")
        cycleSession(1);
    else if (cmd == "hyprlogin:session_prev")
        cycleSession(-1);
    else if (cmd == "hyprlogin:clear_input")
        g_pHyprlock->clearPasswordBuffer();
    else
        return false;

    return true;
}

bool CGreeter::replaceVariables(std::string& in) {
    bool used = false;

    if (in.contains("$GREETD_USER")) {
        // A label showing the user disappears entirely while no user is set.
        if (m_state.targetUsername.empty())
            in = "";
        else
            replaceInString(in, "$GREETD_USER", m_state.targetUsername);
        used = true;
    }

    if (in.contains("$GREETD_PROMPT")) {
        replaceInString(in, "$GREETD_PROMPT", m_state.prompt);
        used = true;
    }

    if (in.contains("$GREETD_SESSION")) {
        replaceInString(in, "$GREETD_SESSION", getSelectedSessionName());
        used = true;
    }

    return used;
}

void CGreeter::onSessionLockReleased() {
    static const auto EXITCOMMAND = g_pConfigManager->getValue<Hyprlang::STRING>("general:exit_command");
    if (const std::string exitCommand = *EXITCOMMAND; !exitCommand.empty()) {
        Log::logger->log(Log::INFO, "Running exit_command '{}'", exitCommand);
        spawnAsync(exitCommand);
    }
}
