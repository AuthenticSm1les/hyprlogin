#include "Auth.hpp"
#include "../greeter/Greetd.hpp"
#include "../config/ConfigManager.hpp"
#include "../core/hyprlock.hpp"
#include "src/helpers/Log.hpp"

#include <algorithm>
#include <hyprlang.hpp>
#include <memory>

CAuth::CAuth() {
    // greetd owns PAM (including pam_fprintd), so it is the only backend.
    m_vImpls.emplace_back(makeShared<CGreetd>());
}

void CAuth::start() {
    Log::logger->debug("auth start: initializing {} implementation(s)", m_vImpls.size());
    for (const auto& i : m_vImpls) {
        i->init();
    }
}

void CAuth::submitInput(const std::string& input) {
    Log::logger->debug("submitInput: {} char(s) to {} implementation(s)", input.size(), m_vImpls.size());
    for (const auto& i : m_vImpls) {
        i->handleInput(input);
    }

    g_pHyprlock->clearPasswordBuffer();
}

bool CAuth::checkWaiting() {
    return std::ranges::any_of(m_vImpls, [](const auto& i) { return i->checkWaiting(); });
}

const std::string& CAuth::getCurrentFailText() {
    return m_sCurrentFail.failText;
}

size_t CAuth::getFailedAttempts() {
    return m_sCurrentFail.failedAttempts;
}

void CAuth::terminate() {
    for (const auto& i : m_vImpls) {
        i->terminate();
    }
}

static void unlockCallback(ASP<CTimer> self, void* data) {
    g_pHyprlock->unlock();
}

void CAuth::enqueueUnlock() {
    g_pHyprlock->addTimer(std::chrono::milliseconds(0), unlockCallback, nullptr);
}

static void passwordFailCallback(ASP<CTimer> self, void* data) {
    g_pAuth->m_bDisplayFailText = true;

    g_pHyprlock->enqueueForceUpdateTimers();

    g_pHyprlock->renderAllOutputs();
}

static void displayFailTimeoutCallback(ASP<CTimer> self, void* data) {
    if (g_pAuth->m_bDisplayFailText) {
        g_pAuth->m_bDisplayFailText = false;
        g_pHyprlock->renderAllOutputs();
    }
}

void CAuth::enqueueFail(const std::string& failText, eAuthImplementations implType) {
    static const auto FAILTIMEOUT = g_pConfigManager->getValue<Hyprlang::INT>("general:fail_timeout");

    m_sCurrentFail.failText   = failText;
    m_sCurrentFail.failSource = implType;
    m_sCurrentFail.failedAttempts++;

    Log::logger->log(Log::INFO, "Failed attempts: {}", m_sCurrentFail.failedAttempts);
    Log::logger->debug("enqueueFail: '{}' from impl={}", failText, (int)implType);

    if (m_resetDisplayFailTimer) {
        m_resetDisplayFailTimer->cancel();
        m_resetDisplayFailTimer.reset();
    }

    g_pHyprlock->addTimer(std::chrono::milliseconds(0), passwordFailCallback, nullptr);
    m_resetDisplayFailTimer = g_pHyprlock->addTimer(std::chrono::milliseconds(*FAILTIMEOUT), displayFailTimeoutCallback, nullptr);
}

void CAuth::resetDisplayFail() {
    g_pAuth->m_bDisplayFailText = false;
    m_resetDisplayFailTimer->cancel();
    m_resetDisplayFailTimer.reset();
}
