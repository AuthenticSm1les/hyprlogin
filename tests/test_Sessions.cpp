#include <gtest/gtest.h>

#include "../src/greeter/Sessions.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <unistd.h>

namespace fs = std::filesystem;

class SessionsTest : public ::testing::Test {
  protected:
    void SetUp() override {
        char tmpl[] = "/tmp/hyprlogin-sessions-XXXXXX";
        ASSERT_NE(mkdtemp(tmpl), nullptr);
        root = tmpl;
        fs::create_directories(root / "bin");
        fs::create_directories(root / "wayland");
        fs::create_directories(root / "x11");

        const char* old = getenv("PATH");
        oldPath         = old ? old : "";
        setenv("PATH", (root / "bin").c_str(), 1);

        makeExecutable("Hyprland");
        makeExecutable("sway");
    }

    void TearDown() override {
        setenv("PATH", oldPath.c_str(), 1);
        std::error_code ec;
        fs::remove_all(root, ec);
    }

    void makeExecutable(const std::string& name) {
        const auto    path = root / "bin" / name;
        std::ofstream(path) << "#!/bin/sh\n";
        fs::permissions(path, fs::perms::owner_all);
    }

    std::string writeDesktop(const std::string& dir, const std::string& file, const std::string& body) {
        const auto path = root / dir / file;
        std::ofstream(path) << body;
        return path.string();
    }

    fs::path    root;
    std::string oldPath;
};

TEST_F(SessionsTest, ParsesValidEntryAndStripsFieldCodes) {
    const auto path    = writeDesktop("wayland", "hyprland.desktop", "[Desktop Entry]\nName=Hyprland\nExec=Hyprland %U\nDesktopNames=Hyprland\n");
    const auto session = Sessions::parseDesktopFile(path, "wayland");
    ASSERT_TRUE(session.has_value());
    EXPECT_EQ(session->name, "Hyprland");
    EXPECT_EQ(session->command, "Hyprland");
    EXPECT_EQ(session->type, "wayland");
    EXPECT_EQ(session->desktopNames, "Hyprland");
    EXPECT_EQ(session->desktopFile, path);
}

TEST_F(SessionsTest, SkipsHiddenNoDisplayAndMissingBinaries) {
    EXPECT_FALSE(Sessions::parseDesktopFile(writeDesktop("wayland", "a.desktop", "[Desktop Entry]\nName=A\nExec=sway\nHidden=true\n"), "wayland"));
    EXPECT_FALSE(Sessions::parseDesktopFile(writeDesktop("wayland", "b.desktop", "[Desktop Entry]\nName=B\nExec=sway\nNoDisplay=true\n"), "wayland"));
    EXPECT_FALSE(Sessions::parseDesktopFile(writeDesktop("wayland", "c.desktop", "[Desktop Entry]\nName=C\nExec=not-installed\n"), "wayland"));
    EXPECT_FALSE(Sessions::parseDesktopFile(writeDesktop("wayland", "d.desktop", "[Desktop Entry]\nName=D\nTryExec=not-installed\nExec=sway\n"), "wayland"));
    EXPECT_FALSE(Sessions::parseDesktopFile(writeDesktop("wayland", "e.desktop", "[Desktop Entry]\nExec=sway\n"), "wayland"));
}

TEST_F(SessionsTest, IgnoresKeysOutsideDesktopEntryGroup) {
    const auto path = writeDesktop("wayland", "s.desktop", "Name=Wrong\n[Desktop Entry]\nName=Sway\nExec=sway\n[Desktop Action x]\nName=Other\n");
    const auto session = Sessions::parseDesktopFile(path, "wayland");
    ASSERT_TRUE(session.has_value());
    EXPECT_EQ(session->name, "Sway");
}

TEST_F(SessionsTest, DiscoverSortsAndTagsTypes) {
    writeDesktop("wayland", "sway.desktop", "[Desktop Entry]\nName=Sway\nExec=sway\n");
    writeDesktop("wayland", "hyprland.desktop", "[Desktop Entry]\nName=Hyprland\nExec=Hyprland\n");
    writeDesktop("x11", "sway-x.desktop", "[Desktop Entry]\nName=Sway X\nExec=sway\n");
    writeDesktop("wayland", "notes.txt", "[Desktop Entry]\nName=Ignored\nExec=sway\n");

    const auto sessions = Sessions::discover((root / "wayland").string(), (root / "x11").string());
    ASSERT_EQ(sessions.size(), 3u);
    EXPECT_EQ(sessions[0].name, "Hyprland");
    EXPECT_EQ(sessions[1].name, "Sway");
    EXPECT_EQ(sessions[2].name, "Sway X");
    EXPECT_EQ(sessions[2].type, "x11");
}

TEST_F(SessionsTest, DiscoverAddsStartxWhenInstalled) {
    EXPECT_TRUE(Sessions::discover("", "").empty());
    makeExecutable("startx");
    const auto sessions = Sessions::discover("", "");
    ASSERT_EQ(sessions.size(), 1u);
    EXPECT_EQ(sessions[0].command, "startx");
}

TEST_F(SessionsTest, DiscoverToleratesMissingDirectories) {
    EXPECT_TRUE(Sessions::discover((root / "nope").string(), (root / "nada").string()).empty());
}

TEST_F(SessionsTest, MatchesByNameCommandPathFileAndStem) {
    const SGreeterSession s{.name = "Hyprland", .command = "start-hyprland", .type = "wayland", .desktopNames = "", .desktopFile = "/usr/share/wayland-sessions/hyprland.desktop"};
    EXPECT_TRUE(Sessions::matches(s, "Hyprland"));
    EXPECT_TRUE(Sessions::matches(s, "start-hyprland"));
    EXPECT_TRUE(Sessions::matches(s, "/usr/share/wayland-sessions/hyprland.desktop"));
    EXPECT_TRUE(Sessions::matches(s, "hyprland.desktop"));
    EXPECT_TRUE(Sessions::matches(s, "hyprland"));
    EXPECT_FALSE(Sessions::matches(s, "sway"));
}

TEST_F(SessionsTest, IdPrefersDesktopFile) {
    EXPECT_EQ(Sessions::id({.name = "A", .command = "a", .type = "wayland", .desktopNames = "", .desktopFile = "/x/a.desktop"}), "/x/a.desktop");
    EXPECT_EQ(Sessions::id({.name = "A", .command = "a", .type = "wayland", .desktopNames = "", .desktopFile = ""}), "A");
}

TEST_F(SessionsTest, EnvironmentForWaylandAndX11) {
    const auto wl = Sessions::environment({.name = "Hyprland", .command = "Hyprland", .type = "wayland", .desktopNames = "Hyprland", .desktopFile = "/x/hyprland.desktop"});
    EXPECT_EQ(wl, (std::vector<std::string>{"XDG_SESSION_TYPE=wayland", "XDG_SESSION_DESKTOP=Hyprland", "DESKTOP_SESSION=hyprland", "XDG_CURRENT_DESKTOP=Hyprland"}));

    const auto x = Sessions::environment({.name = "i3", .command = "i3", .type = "x11", .desktopNames = "", .desktopFile = "/x/i3.desktop"});
    EXPECT_EQ(x, (std::vector<std::string>{"XDG_SESSION_TYPE=x11", "XDG_SESSION_DESKTOP=i3", "DESKTOP_SESSION=i3"}));
}

TEST_F(SessionsTest, CurrentDesktopUsesColonSeparators) {
    const auto env = Sessions::environment({.name = "GNOME", .command = "gnome-session", .type = "wayland", .desktopNames = "GNOME;Unity;", .desktopFile = "/x/gnome.desktop"});
    EXPECT_EQ(env.back(), "XDG_CURRENT_DESKTOP=GNOME:Unity");
}

TEST_F(SessionsTest, IsExecutableCommand) {
    EXPECT_TRUE(Sessions::isExecutableCommand("sway"));
    EXPECT_TRUE(Sessions::isExecutableCommand("  sway --unsupported-gpu"));
    EXPECT_TRUE(Sessions::isExecutableCommand("WLR_NO_HARDWARE_CURSORS=1 FOO=bar sway"));
    EXPECT_TRUE(Sessions::isExecutableCommand((root / "bin" / "sway").string()));
    EXPECT_FALSE(Sessions::isExecutableCommand(""));
    EXPECT_FALSE(Sessions::isExecutableCommand("   "));
    EXPECT_FALSE(Sessions::isExecutableCommand("FOO=bar"));
    EXPECT_FALSE(Sessions::isExecutableCommand("not-installed"));
    EXPECT_FALSE(Sessions::isExecutableCommand((root / "bin" / "missing").string()));
}
