#include <gtest/gtest.h>
#include <string>
#include <filesystem>
#include <cstdlib>

namespace {

std::string absolutePath(const std::string& rawpath, const std::string& currentDir) {
    std::filesystem::path path(rawpath);

    if (!rawpath.empty() && rawpath[0] == '~') {
        static const char* const ENVHOME = std::getenv("HOME");
        path = std::filesystem::path(ENVHOME) / path.relative_path().string().substr(2);
    }

    if (path.is_relative()) {
        return std::filesystem::weakly_canonical(std::filesystem::path(currentDir) / path);
    } else {
        return std::filesystem::weakly_canonical(path);
    }
}

bool isExecutableCommand(const std::string& exec) {
    if (exec.empty())
        return false;

    size_t pos = exec.find_first_not_of(" \t");
    if (pos == std::string::npos)
        return false;

    std::string firstToken;
    while (true) {
        auto end = exec.find_first_of(" \t", pos);
        firstToken = exec.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        if (firstToken.empty())
            return false;
        if (!firstToken.contains("="))
            break;
        if (end == std::string::npos)
            return false;
        pos = exec.find_first_not_of(" \t", end);
        if (pos == std::string::npos)
            return false;
    }

    if (firstToken.starts_with('/'))
        return std::filesystem::exists(firstToken) && std::filesystem::is_regular_file(firstToken);

    const auto* pathEnv = std::getenv("PATH");
    if (!pathEnv)
        return false;

    std::string paths = pathEnv;
    size_t      start = 0;
    while (start <= paths.size()) {
        const auto end   = paths.find(':', start);
        const auto entry = paths.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!entry.empty()) {
            const auto candidate = std::filesystem::path(entry) / firstToken;
            if (std::filesystem::exists(candidate) && std::filesystem::is_regular_file(candidate))
                return true;
        }

        if (end == std::string::npos)
            break;
        start = end + 1;
    }

    return false;
}

}

TEST(AbsolutePathTest, AbsolutePathRelativeCurrentDir) {
    std::string result = absolutePath("file.txt", "/tmp");
    EXPECT_EQ(result, "/tmp/file.txt");
}

TEST(AbsolutePathTest, AbsolutePathRelativeDot) {
    std::string result = absolutePath("./foo", "/tmp");
    EXPECT_EQ(result, "/tmp/foo");
}

TEST(AbsolutePathTest, AbsolutePathRelativeParentDir) {
    std::string result = absolutePath("../bar", "/tmp/sub");
    EXPECT_EQ(result, "/tmp/bar");
}

TEST(AbsolutePathTest, AbsolutePathAbsolute) {
    std::string result = absolutePath("/usr/bin", "/tmp");
    EXPECT_EQ(result, "/usr/bin");
}

TEST(AbsolutePathTest, AbsolutePathHomeExpansion) {
    setenv("HOME", "/home/user", 1);
    std::string result = absolutePath("~/some/path", "/tmp");
    EXPECT_EQ(result, "/home/user/some/path");
}

TEST(AbsolutePathTest, AbsolutePathHomeExpansionSimple) {
    std::string result = absolutePath("~/.config", "/tmp");
    EXPECT_EQ(result, "/home/user/.config");
}

TEST(AbsolutePathTest, AbsolutePathHomeExpansionParentFromHome) {
    std::string result = absolutePath("~/..", "/tmp");
    EXPECT_EQ(result, "/home/");
}

TEST(IsExecutableCommandTest, EmptyReturnsFalse) {
    EXPECT_FALSE(isExecutableCommand(""));
}

TEST(IsExecutableCommandTest, AbsolutePathToBinSh) {
    EXPECT_TRUE(isExecutableCommand("/usr/bin/bash"));
}

TEST(IsExecutableCommandTest, AbsolutePathNonExistent) {
    EXPECT_FALSE(isExecutableCommand("/nonexistent/path/binary"));
}

TEST(IsExecutableCommandTest, CommandFromPath) {
    EXPECT_TRUE(isExecutableCommand("sh"));
    EXPECT_TRUE(isExecutableCommand("ls"));
}

TEST(IsExecutableCommandTest, NonExistentCommand) {
    EXPECT_FALSE(isExecutableCommand("xyznonexistentcmd123"));
}

TEST(IsExecutableCommandTest, WithEnvPrefix) {
    EXPECT_TRUE(isExecutableCommand("FOO=1 ls"));
    EXPECT_TRUE(isExecutableCommand("BAR=1 BAZ=2 /usr/bin/sh"));
}
