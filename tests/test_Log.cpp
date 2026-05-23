#include <gtest/gtest.h>
#include "../src/helpers/Log.hpp"

TEST(LogTest, LoggerDefaults) {
    Log::CLogger logger;
    EXPECT_FALSE(logger.verbose());
}

TEST(LogTest, SetVerbose) {
    Log::CLogger logger;
    logger.setVerbose();
    EXPECT_TRUE(logger.verbose());
}

TEST(LogTest, SetVerboseThenUnverbose) {
    Log::CLogger logger;
    logger.setVerbose();
    EXPECT_TRUE(logger.verbose());
}

TEST(LogTest, SetQuiet) {
    Log::CLogger logger;
    logger.setQuiet();
    EXPECT_FALSE(logger.verbose());
}
