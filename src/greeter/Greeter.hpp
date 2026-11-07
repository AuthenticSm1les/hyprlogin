#pragma once

#include "Sessions.hpp"
#include "../defines.hpp"

#include <hyprlang.hpp>
#include <string>
#include <vector>
#include <xkbcommon/xkbcommon.h>

// Everything hyprlogin adds on top of hyprlock lives in src/greeter. Upstream
// files only call into g_pGreeter from a few hooks marked "// hyprlogin".
class CGreeter {
  public:
    // Registers the sessions:* options and the greeter additions to
    // general, input-field and label. Call before CConfig::commence().
    static void registerConfig(Hyprlang::CConfig& config);

    // Discovers sessions and preselects the remembered/default user and session.
    void init();

    // UI state shown by widgets ($GREETD_* variables, input field).
    void               setPrompt(const std::string& prompt, bool secretInput);
    const std::string& getPrompt();
    bool               isSecretInput();
    void               setTargetUsername(const std::string& username);
    void               clearTargetUsername();
    const std::string& getTargetUsername();
    // Updates prompt, input mode and user together and renders once, so the
    // prompt never shows a stale value next to an already changed user.
    void setUIState(const std::string& prompt, bool secretInput, const std::string& username);

    void                     cycleSession(int delta);
    std::string              getSelectedSessionName();
    std::string              getSelectedSessionCommand();
    std::vector<std::string> getSelectedSessionEnv();

    // sessions:default_user, overridden by the remembered user when
    // sessions:remember_user is on and a previous login was stored.
    const std::string& getDefaultUser();
    // Stores user and selected session for the next greeter run.
    void rememberLogin(const std::string& username);

    // Hooks called from upstream code.
    bool handleKeySym(xkb_keysym_t sym);                  // true if consumed
    bool handleInternalCommand(const std::string& cmd);   // onclick = hyprlogin:...
    bool replaceVariables(std::string& in);               // true if a $GREETD_* variable was used
    void onSessionLockReleased();                         // runs general:exit_command

  private:
    void refresh();

    struct {
        std::string                  prompt               = "Username";
        bool                         secretInput          = false;
        std::string                  targetUsername       = "";
        std::vector<SGreeterSession> sessions             = {};
        size_t                       selectedSessionIndex = 0;
        std::string                  defaultUser          = "";
    } m_state;
};

inline UP<CGreeter> g_pGreeter;
