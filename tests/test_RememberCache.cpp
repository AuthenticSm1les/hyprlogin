#include <gtest/gtest.h>

#include "../src/greeter/RememberCache.hpp"

#include <filesystem>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;

class RememberCacheTest : public ::testing::Test {
  protected:
    void SetUp() override {
        char tmpl[] = "/tmp/hyprlogin-remember-XXXXXX";
        ASSERT_NE(mkdtemp(tmpl), nullptr);
        dir = tmpl;
    }

    void TearDown() override {
        std::error_code ec;
        fs::permissions(dir, fs::perms::owner_all, ec);
        fs::remove_all(dir, ec);
    }

    void writeRaw(const std::string& entry, const std::string& content) {
        std::ofstream out(fs::path{dir} / entry, std::ios::binary);
        out << content;
    }

    std::string dir;
};

TEST_F(RememberCacheTest, MissingEntryIsNullopt) {
    EXPECT_FALSE(RememberCache::read(dir, RememberCache::USER_ENTRY).has_value());
}

TEST_F(RememberCacheTest, MissingDirIsNullopt) {
    EXPECT_FALSE(RememberCache::read(dir + "/nope", RememberCache::USER_ENTRY).has_value());
    EXPECT_FALSE(RememberCache::read("", RememberCache::USER_ENTRY).has_value());
}

TEST_F(RememberCacheTest, RoundTrip) {
    ASSERT_TRUE(RememberCache::write(dir, RememberCache::USER_ENTRY, "smiley"));
    ASSERT_TRUE(RememberCache::write(dir, RememberCache::SESSION_ENTRY, "/usr/share/wayland-sessions/hyprland.desktop"));

    EXPECT_EQ(RememberCache::read(dir, RememberCache::USER_ENTRY), "smiley");
    EXPECT_EQ(RememberCache::read(dir, RememberCache::SESSION_ENTRY), "/usr/share/wayland-sessions/hyprland.desktop");
}

TEST_F(RememberCacheTest, OverwriteReplacesValue) {
    ASSERT_TRUE(RememberCache::write(dir, RememberCache::USER_ENTRY, "alice"));
    ASSERT_TRUE(RememberCache::write(dir, RememberCache::USER_ENTRY, "bob"));
    EXPECT_EQ(RememberCache::read(dir, RememberCache::USER_ENTRY), "bob");
}

TEST_F(RememberCacheTest, WriteLeavesNoTempFile) {
    ASSERT_TRUE(RememberCache::write(dir, RememberCache::USER_ENTRY, "smiley"));
    size_t count = 0;
    for ([[maybe_unused]] const auto& e : fs::directory_iterator(dir))
        ++count;
    EXPECT_EQ(count, 1u);
}

TEST_F(RememberCacheTest, TrailingNewlinesAreStripped) {
    writeRaw(RememberCache::USER_ENTRY, "smiley\r\n\n");
    EXPECT_EQ(RememberCache::read(dir, RememberCache::USER_ENTRY), "smiley");
}

TEST_F(RememberCacheTest, CorruptEntriesAreRejected) {
    writeRaw(RememberCache::USER_ENTRY, "");
    EXPECT_FALSE(RememberCache::read(dir, RememberCache::USER_ENTRY).has_value());

    writeRaw(RememberCache::USER_ENTRY, "two\nlines");
    EXPECT_FALSE(RememberCache::read(dir, RememberCache::USER_ENTRY).has_value());

    writeRaw(RememberCache::USER_ENTRY, std::string("nul\0byte", 8));
    EXPECT_FALSE(RememberCache::read(dir, RememberCache::USER_ENTRY).has_value());

    writeRaw(RememberCache::USER_ENTRY, std::string(5000, 'a'));
    EXPECT_FALSE(RememberCache::read(dir, RememberCache::USER_ENTRY).has_value());
}

TEST_F(RememberCacheTest, InvalidValuesAreNotWritten) {
    ASSERT_TRUE(RememberCache::write(dir, RememberCache::USER_ENTRY, "smiley"));
    EXPECT_FALSE(RememberCache::write(dir, RememberCache::USER_ENTRY, ""));
    EXPECT_FALSE(RememberCache::write(dir, RememberCache::USER_ENTRY, "evil\nuser"));
    EXPECT_FALSE(RememberCache::write("", RememberCache::USER_ENTRY, "smiley"));
    EXPECT_EQ(RememberCache::read(dir, RememberCache::USER_ENTRY), "smiley");
}

TEST_F(RememberCacheTest, UnwritableDirFailsAndKeepsOldValue) {
    if (geteuid() == 0)
        GTEST_SKIP() << "root ignores directory permissions";

    ASSERT_TRUE(RememberCache::write(dir, RememberCache::USER_ENTRY, "smiley"));
    fs::permissions(dir, fs::perms::owner_read | fs::perms::owner_exec);
    EXPECT_FALSE(RememberCache::write(dir, RememberCache::USER_ENTRY, "other"));
    EXPECT_EQ(RememberCache::read(dir, RememberCache::USER_ENTRY), "smiley");
}
