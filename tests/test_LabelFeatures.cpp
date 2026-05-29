#include <gtest/gtest.h>
#include <hyprutils/string/String.hpp>
#include <string>

using namespace Hyprutils::String;

TEST(LabelFeaturesTest, ReplaceInString) {
    std::string text = "Login as $GREETD_USER";
    replaceInString(text, "$GREETD_USER", "smiley");
    EXPECT_EQ(text, "Login as smiley");
}

TEST(LabelFeaturesTest, ReplaceInStringEmptyReplacement) {
    std::string text = "Login as $GREETD_USER";
    replaceInString(text, "$GREETD_USER", "");
    EXPECT_EQ(text, "Login as ");
}

TEST(LabelFeaturesTest, ReplaceInStringNotFound) {
    std::string text = "Hello, world!";
    replaceInString(text, "$GREETD_USER", "smiley");
    EXPECT_EQ(text, "Hello, world!");
}

TEST(LabelFeaturesTest, ReplaceInStringEmptySource) {
    std::string text = "";
    replaceInString(text, "$GREETD_USER", "smiley");
    EXPECT_EQ(text, "");
}

TEST(LabelFeaturesTest, ReplaceInStringMultiple) {
    std::string text = "$GREETD_USER and $GREETD_USER";
    replaceInString(text, "$GREETD_USER", "smiley");
    EXPECT_EQ(text, "smiley and smiley");
}

TEST(LabelFeaturesTest, BlankCheck) {
    auto isBlank = [](const std::string& s) {
        return s.find_first_not_of(" \t\n\r\f\v") == std::string::npos;
    };

    EXPECT_TRUE(isBlank(""));
    EXPECT_TRUE(isBlank("   "));
    EXPECT_TRUE(isBlank("\t"));
    EXPECT_TRUE(isBlank("\n"));
    EXPECT_TRUE(isBlank(" \t\n\r\f\v"));
    EXPECT_FALSE(isBlank("a"));
    EXPECT_FALSE(isBlank(" Login as "));
    EXPECT_FALSE(isBlank("$GREETD_USER"));
}

TEST(LabelFeaturesTest, SFormatResultDefaults) {
    struct SFormatResult {
        std::string formatted;
        float       updateEveryMs    = 0;
        bool        alwaysUpdate     = false;
        bool        cmd              = false;
        bool        allowForceUpdate = false;
    };

    SFormatResult r;
    EXPECT_TRUE(r.formatted.empty());
    EXPECT_FALSE(r.allowForceUpdate);
    EXPECT_FALSE(r.cmd);
    EXPECT_FALSE(r.alwaysUpdate);
    EXPECT_EQ(r.updateEveryMs, 0);
}

TEST(LabelFeaturesTest, SFormatResultAllowForceUpdate) {
    struct SFormatResult {
        std::string formatted;
        float       updateEveryMs    = 0;
        bool        alwaysUpdate     = false;
        bool        cmd              = false;
        bool        allowForceUpdate = false;
    };

    SFormatResult r;
    r.formatted         = "Login as smiley";
    r.allowForceUpdate  = true;
    EXPECT_TRUE(r.allowForceUpdate);
    EXPECT_EQ(r.formatted, "Login as smiley");
}
