#include <gtest/gtest.h>
#include <string>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace {

std::string trimWhitespace(std::string value) {
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), value.end());
    return value;
}

std::string sanitizeDesktopExec(const std::string& exec) {
    std::string sanitized;
    sanitized.reserve(exec.size());

    for (size_t i = 0; i < exec.size(); ++i) {
        if (exec[i] == '%' && i + 1 < exec.size() && std::isalpha(static_cast<unsigned char>(exec[i + 1]))) {
            ++i;
            continue;
        }
        sanitized.push_back(exec[i]);
    }

    return trimWhitespace(sanitized);
}

std::string getDesktopEntryValue(const std::filesystem::path& path, const std::string& key) {
    std::ifstream file(path);
    if (!file.is_open())
        return "";

    std::string line;
    bool        inDesktopEntry = false;
    while (std::getline(file, line)) {
        line = trimWhitespace(line);
        if (line.empty() || line.starts_with("#"))
            continue;

        if (line == "[Desktop Entry]") {
            inDesktopEntry = true;
            continue;
        }

        if (!inDesktopEntry)
            continue;

        if (line.starts_with('['))
            break;

        const auto pos = line.find('=');
        if (pos == std::string::npos)
            continue;

        if (line.substr(0, pos) == key)
            return trimWhitespace(line.substr(pos + 1));
    }

    return "";
}

}

TEST(TrimWhitespaceTest, NoWhitespace) {
    EXPECT_EQ(trimWhitespace("hello"), "hello");
}

TEST(TrimWhitespaceTest, LeadingSpaces) {
    EXPECT_EQ(trimWhitespace("   hello"), "hello");
}

TEST(TrimWhitespaceTest, TrailingSpaces) {
    EXPECT_EQ(trimWhitespace("hello   "), "hello");
}

TEST(TrimWhitespaceTest, LeadingAndTrailing) {
    EXPECT_EQ(trimWhitespace("  hello world  "), "hello world");
}

TEST(TrimWhitespaceTest, Tabs) {
    EXPECT_EQ(trimWhitespace("\thello\t"), "hello");
}

TEST(TrimWhitespaceTest, MixedWhitespace) {
    EXPECT_EQ(trimWhitespace(" \t \nhello \t\n "), "hello");
}

TEST(TrimWhitespaceTest, OnlyWhitespace) {
    EXPECT_EQ(trimWhitespace("   \t  \n  "), "");
}

TEST(TrimWhitespaceTest, EmptyString) {
    EXPECT_EQ(trimWhitespace(""), "");
}

TEST(SanitizeDesktopExecTest, NoPlaceholders) {
    EXPECT_EQ(sanitizeDesktopExec("firefox"), "firefox");
}

TEST(SanitizeDesktopExecTest, StripPlaceholders) {
    EXPECT_EQ(sanitizeDesktopExec("firefox %u"), "firefox");
}

TEST(SanitizeDesktopExecTest, StripMultiplePlaceholders) {
    EXPECT_EQ(sanitizeDesktopExec("firefox %U %f"), "firefox");
}

TEST(SanitizeDesktopExecTest, StripFlagPlaceholders) {
    EXPECT_EQ(sanitizeDesktopExec("kitty %F"), "kitty");
}

TEST(SanitizeDesktopExecTest, PercentNotFollowedByLetter) {
    EXPECT_EQ(sanitizeDesktopExec("100% done"), "100% done");
}

TEST(SanitizeDesktopExecTest, MixedContent) {
    EXPECT_EQ(sanitizeDesktopExec("  alacritty -e bash %u  "), "alacritty -e bash");
}

TEST(SanitizeDesktopExecTest, OnlyPlaceholder) {
    EXPECT_EQ(sanitizeDesktopExec("%f"), "");
}

TEST(SanitizeDesktopExecTest, MultipleTokensWithoutSpaces) {
    EXPECT_EQ(sanitizeDesktopExec("wezterm%Ustart"), "weztermstart");
}

TEST(GetDesktopEntryValueTest, FileNotFound) {
    auto result = getDesktopEntryValue("/nonexistent/path/foo.desktop", "Name");
    EXPECT_EQ(result, "");
}

TEST(GetDesktopEntryValueTest, CorrectKey) {
    auto path = (std::filesystem::temp_directory_path() / "test_app.desktop");
    {
        std::ofstream f(path);
        f << "[Desktop Entry]\n";
        f << "Name=TestApp\n";
        f << "Exec=/usr/bin/testapp\n";
        f << "Type=Application\n";
    }
    EXPECT_EQ(getDesktopEntryValue(path, "Name"), "TestApp");
    EXPECT_EQ(getDesktopEntryValue(path, "Exec"), "/usr/bin/testapp");
    EXPECT_EQ(getDesktopEntryValue(path, "Type"), "Application");
    std::filesystem::remove(path);
}

TEST(GetDesktopEntryValueTest, KeyNotFound) {
    auto path = (std::filesystem::temp_directory_path() / "test_app2.desktop");
    {
        std::ofstream f(path);
        f << "[Desktop Entry]\n";
        f << "Name=TestApp\n";
    }
    EXPECT_EQ(getDesktopEntryValue(path, "NonExistent"), "");
    std::filesystem::remove(path);
}

TEST(GetDesktopEntryValueTest, OnlyDesktopEntrySection) {
    auto path = (std::filesystem::temp_directory_path() / "test_app3.desktop");
    {
        std::ofstream f(path);
        f << "[Desktop Entry]\n";
        f << "Name=MyApp\n";
        f << "[Action Menu]\n";
        f << "Name=Other\n";
    }
    EXPECT_EQ(getDesktopEntryValue(path, "Name"), "MyApp");
    std::filesystem::remove(path);
}

TEST(GetDesktopEntryValueTest, CommentLines) {
    auto path = (std::filesystem::temp_directory_path() / "test_app4.desktop");
    {
        std::ofstream f(path);
        f << "[Desktop Entry]\n";
        f << "# This is a comment\n";
        f << "Name=RealName\n";
    }
    EXPECT_EQ(getDesktopEntryValue(path, "Name"), "RealName");
    std::filesystem::remove(path);
}

TEST(GetDesktopEntryValueTest, ValueWithWhitespace) {
    auto path = (std::filesystem::temp_directory_path() / "test_app5.desktop");
    {
        std::ofstream f(path);
        f << "[Desktop Entry]\n";
        f << "Name=  My App  \n";
    }
    EXPECT_EQ(getDesktopEntryValue(path, "Name"), "My App");
    std::filesystem::remove(path);
}

namespace {

struct TM {
    int64_t hrs;
    int64_t mins;
};

std::string getTime24h(const TM& t) {
    return (t.hrs < 10 ? "0" : "") + std::to_string(t.hrs) + ":" + (t.mins < 10 ? "0" : "") + std::to_string(t.mins);
}

std::string getTime12h(const TM& t) {
    return (t.hrs == 12 || t.hrs == 0 ? "12" : (t.hrs % 12 < 10 ? "0" : "") + std::to_string(t.hrs % 12)) + ":" +
        (t.mins < 10 ? "0" : "") + std::to_string(t.mins) + (t.hrs < 12 ? " AM" : " PM");
}

}

TEST(TimeFormat24hTest, Midnight) {
    EXPECT_EQ(getTime24h({0, 0}), "00:00");
}

TEST(TimeFormat24hTest, Noon) {
    EXPECT_EQ(getTime24h({12, 0}), "12:00");
}

TEST(TimeFormat24hTest, Morning) {
    EXPECT_EQ(getTime24h({9, 5}), "09:05");
}

TEST(TimeFormat24hTest, Afternoon) {
    EXPECT_EQ(getTime24h({15, 30}), "15:30");
}

TEST(TimeFormat24hTest, LateNight) {
    EXPECT_EQ(getTime24h({23, 59}), "23:59");
}

TEST(TimeFormat12hTest, Midnight) {
    EXPECT_EQ(getTime12h({0, 0}), "12:00 AM");
}

TEST(TimeFormat12hTest, Noon) {
    EXPECT_EQ(getTime12h({12, 0}), "12:00 PM");
}

TEST(TimeFormat12hTest, Morning) {
    EXPECT_EQ(getTime12h({9, 5}), "09:05 AM");
}

TEST(TimeFormat12hTest, Afternoon) {
    EXPECT_EQ(getTime12h({15, 30}), "03:30 PM");
}

TEST(TimeFormat12hTest, LateNight) {
    EXPECT_EQ(getTime12h({23, 45}), "11:45 PM");
}

TEST(TimeFormat12hTest, EarlyMorning) {
    EXPECT_EQ(getTime12h({1, 15}), "01:15 AM");
}

TEST(TimeFormat12hTest, OnePm) {
    EXPECT_EQ(getTime12h({13, 0}), "01:00 PM");
}
