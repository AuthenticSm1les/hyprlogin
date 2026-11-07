#include <gtest/gtest.h>

#include "../src/greeter/Logging.hpp"

#include <filesystem>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {
    // Opens and immediately closes; true if openLogFile accepted the path.
    bool opens(const fs::path& p) {
        const auto fd = GreeterLogging::openLogFile(p);
        if (fd)
            close(*fd);
        return fd.has_value();
    }
}

class LoggingTest : public ::testing::Test {
  protected:
    void SetUp() override {
        char tmpl[] = "/tmp/hyprlogin-logging-XXXXXX";
        ASSERT_NE(mkdtemp(tmpl), nullptr);
        dir = tmpl;
    }

    void TearDown() override {
        fs::remove_all(dir);
    }

    static mode_t modeOf(const fs::path& p) {
        struct stat st{};
        EXPECT_EQ(lstat(p.c_str(), &st), 0);
        return st.st_mode & 0777;
    }

    fs::path dir;
};

TEST_F(LoggingTest, CreatesPrivateFile) {
    const auto path = dir / "log";
    ASSERT_TRUE(opens(path));
    EXPECT_TRUE(fs::is_regular_file(path));
    EXPECT_EQ(modeOf(path), 0600);
}

TEST_F(LoggingTest, TightensExistingFileAndKeepsContents) {
    const auto path = dir / "log";
    std::ofstream(path) << "old\n";
    fs::permissions(path, fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read | fs::perms::others_read);

    ASSERT_TRUE(opens(path));
    EXPECT_EQ(modeOf(path), 0600);
    EXPECT_EQ(fs::file_size(path), 4u);
}

TEST_F(LoggingTest, RejectsSymlink) {
    const auto target = dir / "target";
    const auto link   = dir / "link";
    std::ofstream(target) << "";
    fs::create_symlink(target, link);

    EXPECT_FALSE(opens(link));
}

TEST_F(LoggingTest, RejectsDirectory) {
    EXPECT_FALSE(opens(dir));
}

TEST_F(LoggingTest, RejectsMissingParent) {
    EXPECT_FALSE(opens(dir / "missing" / "log"));
}

TEST_F(LoggingTest, AppendsAndKeepsLinesOnAbortPath) {
    const auto path = dir / "log";
    std::ofstream(path) << "old\n";

    const auto fd = GreeterLogging::openLogFile(path);
    ASSERT_TRUE(fd.has_value());
    ASSERT_EQ(write(*fd, "new\n", 4), 4);
    close(*fd);

    std::ifstream in(path);
    std::string   all((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(all, "old\nnew\n");
}
