#include <gtest/gtest.h>
#include <string>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace {

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t");
    if (start == std::string::npos)
        return "";
    auto end = s.find_last_not_of(" \t");
    return s.substr(start, end - start + 1);
}

int64_t configStringToInt(const std::string& VALUE) {
    auto parseHex = [](const std::string& value) -> int64_t {
        try {
            size_t position;
            auto   result = std::stoll(value, &position, 16);
            if (position == value.size())
                return result;
        } catch (const std::exception&) {}
        throw std::invalid_argument("invalid hex " + value);
    };

    if (VALUE.starts_with("0x")) {
        return parseHex(VALUE);
    } else if (VALUE.starts_with("rgba(") && VALUE.ends_with(')')) {
        const auto VALUEWITHOUTFUNC = trim(VALUE.substr(5, VALUE.length() - 6));

        if (std::count(VALUEWITHOUTFUNC.begin(), VALUEWITHOUTFUNC.end(), ',') == 3) {
            std::string rolling = VALUEWITHOUTFUNC;
            auto        r       = configStringToInt(trim(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            auto g              = configStringToInt(trim(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            auto b              = configStringToInt(trim(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            uint8_t a           = 0;
            try {
                a = std::round(std::stof(trim(rolling.substr(0, rolling.find(',')))) * 255.f);
            } catch (std::exception& e) { throw std::invalid_argument("failed parsing " + VALUEWITHOUTFUNC); }

            return (a * static_cast<int64_t>(0x1000000)) + (r * static_cast<int64_t>(0x10000)) + (g * static_cast<int64_t>(0x100)) + b;
        } else if (VALUEWITHOUTFUNC.length() == 8) {
            const auto RGBA = parseHex(VALUEWITHOUTFUNC);
            return (RGBA >> 8) + (0x1000000 * (RGBA & 0xFF));
        }

        throw std::invalid_argument("rgba() expects length of 8 characters (4 bytes) or 4 comma separated values");

    } else if (VALUE.starts_with("rgb(") && VALUE.ends_with(')')) {
        const auto VALUEWITHOUTFUNC = trim(VALUE.substr(4, VALUE.length() - 5));

        if (std::count(VALUEWITHOUTFUNC.begin(), VALUEWITHOUTFUNC.end(), ',') == 2) {
            std::string rolling = VALUEWITHOUTFUNC;
            auto        r       = configStringToInt(trim(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            auto g              = configStringToInt(trim(rolling.substr(0, rolling.find(','))));
            rolling             = rolling.substr(rolling.find(',') + 1);
            auto b              = configStringToInt(trim(rolling.substr(0, rolling.find(','))));

            return static_cast<int64_t>(0xFF000000) + (r * static_cast<int64_t>(0x10000)) + (g * static_cast<int64_t>(0x100)) + b;
        } else if (VALUEWITHOUTFUNC.length() == 6) {
            return parseHex(VALUEWITHOUTFUNC) + 0xFF000000;
        }

        throw std::invalid_argument("rgb() expects length of 6 characters (3 bytes) or 3 comma separated values");
    } else if (VALUE.starts_with("true") || VALUE.starts_with("on") || VALUE.starts_with("yes")) {
        return 1;
    } else if (VALUE.starts_with("false") || VALUE.starts_with("off") || VALUE.starts_with("no")) {
        return 0;
    }

    try {
        const auto RES = std::stoll(VALUE);
        return RES;
    } catch (const std::invalid_argument&) { throw; } catch (std::exception& e) {
        throw std::invalid_argument(std::string{"stoll threw: "} + e.what());
    }

    return 0;
}

}

TEST(MiscFunctionsTest, ConfigStringToIntPlainDecimal) {
    EXPECT_EQ(configStringToInt("42"), 42);
    EXPECT_EQ(configStringToInt("0"), 0);
    EXPECT_EQ(configStringToInt("-10"), -10);
    EXPECT_EQ(configStringToInt("255"), 255);
}

TEST(MiscFunctionsTest, ConfigStringToIntHex) {
    EXPECT_EQ(configStringToInt("0xFF"), 255);
    EXPECT_EQ(configStringToInt("0xff"), 255);
    EXPECT_EQ(configStringToInt("0x00"), 0);
    EXPECT_EQ(configStringToInt("0x10"), 16);
    EXPECT_EQ(configStringToInt("0xABCDEF"), 11259375);
}

TEST(MiscFunctionsTest, ConfigStringToIntRgb6Char) {
    int64_t result = configStringToInt("rgb(ff0000)");
    EXPECT_EQ((result >> 24) & 0xFF, 0xFF);
    EXPECT_EQ((result >> 16) & 0xFF, 0xFF);
    EXPECT_EQ((result >> 8) & 0xFF, 0x00);
    EXPECT_EQ(result & 0xFF, 0x00);
}

TEST(MiscFunctionsTest, ConfigStringToIntRgbCommas) {
    int64_t result = configStringToInt("rgb(255, 0, 0)");
    EXPECT_EQ((result >> 24) & 0xFF, 0xFF);
    EXPECT_EQ((result >> 16) & 0xFF, 0xFF);
    EXPECT_EQ((result >> 8) & 0xFF, 0x00);
    EXPECT_EQ(result & 0xFF, 0x00);
}

TEST(MiscFunctionsTest, ConfigStringToIntRgba8Char) {
    int64_t result = configStringToInt("rgba(ff000080)");
    EXPECT_EQ((result >> 24) & 0xFF, 0x80);
    EXPECT_EQ((result >> 16) & 0xFF, 0xFF);
    EXPECT_EQ((result >> 8) & 0xFF, 0x00);
    EXPECT_EQ(result & 0xFF, 0x00);
}

TEST(MiscFunctionsTest, ConfigStringToIntRgbaCommas) {
    int64_t result = configStringToInt("rgba(255, 0, 0, 0.5)");
    EXPECT_EQ((result >> 24) & 0xFF, 0x80);
    EXPECT_EQ((result >> 16) & 0xFF, 0xFF);
    EXPECT_EQ((result >> 8) & 0xFF, 0x00);
    EXPECT_EQ(result & 0xFF, 0x00);
}

TEST(MiscFunctionsTest, ConfigStringToIntBooleanTrue) {
    EXPECT_EQ(configStringToInt("true"), 1);
    EXPECT_EQ(configStringToInt("on"), 1);
    EXPECT_EQ(configStringToInt("yes"), 1);
}

TEST(MiscFunctionsTest, ConfigStringToIntBooleanFalse) {
    EXPECT_EQ(configStringToInt("false"), 0);
    EXPECT_EQ(configStringToInt("off"), 0);
    EXPECT_EQ(configStringToInt("no"), 0);
}

TEST(MiscFunctionsTest, ConfigStringToIntStartsWithNoReturnsZero) {
    EXPECT_EQ(configStringToInt("not_a_number"), 0);
}

TEST(MiscFunctionsTest, ConfigStringToIntThrowsOnEmpty) {
    EXPECT_THROW(configStringToInt(""), std::invalid_argument);
}

TEST(MiscFunctionsTest, ConfigStringToIntThrowsOnBadHex) {
    EXPECT_THROW(configStringToInt("0xGGG"), std::invalid_argument);
}

TEST(MiscFunctionsTest, ConfigStringToIntThrowsOnBadRgb) {
    EXPECT_THROW(configStringToInt("rgb(xyz)"), std::invalid_argument);
}

TEST(MiscFunctionsTest, ConfigStringToIntRgbGreen) {
    int64_t result = configStringToInt("rgb(00ff00)");
    EXPECT_EQ((result >> 16) & 0xFF, 0x00);
    EXPECT_EQ((result >> 8) & 0xFF, 0xFF);
    EXPECT_EQ(result & 0xFF, 0x00);
}

TEST(MiscFunctionsTest, ConfigStringToIntRgbBlue) {
    int64_t result = configStringToInt("rgb(0000ff)");
    EXPECT_EQ((result >> 16) & 0xFF, 0x00);
    EXPECT_EQ((result >> 8) & 0xFF, 0x00);
    EXPECT_EQ(result & 0xFF, 0xFF);
}

TEST(MiscFunctionsTest, ConfigStringToIntRgbaCommasGreen) {
    int64_t result = configStringToInt("rgba(0, 255, 0, 1.0)");
    EXPECT_EQ((result >> 24) & 0xFF, 0xFF);
    EXPECT_EQ((result >> 16) & 0xFF, 0x00);
    EXPECT_EQ((result >> 8) & 0xFF, 0xFF);
    EXPECT_EQ(result & 0xFF, 0x00);
}

TEST(MiscFunctionsTest, ConfigStringToIntPlainDecimalLarge) {
    EXPECT_EQ(configStringToInt("999999"), 999999);
    EXPECT_EQ(configStringToInt("2147483647"), 2147483647);
}

TEST(MiscFunctionsTest, ConfigStringToIntHexLarge) {
    EXPECT_EQ(configStringToInt("0xFFFFFFFF"), 4294967295LL);
}

TEST(MiscFunctionsTest, ConfigStringToIntRgbAllChannels) {
    int64_t result = configStringToInt("rgb(10, 20, 30)");
    EXPECT_EQ((result >> 24) & 0xFF, 0xFF);
    EXPECT_EQ((result >> 16) & 0xFF, 10);
    EXPECT_EQ((result >> 8) & 0xFF, 20);
    EXPECT_EQ(result & 0xFF, 30);
}

TEST(MiscFunctionsTest, ConfigStringToIntRgbaAllChannels) {
    int64_t result = configStringToInt("rgba(10, 20, 30, 1.0)");
    EXPECT_EQ((result >> 24) & 0xFF, 0xFF);
    EXPECT_EQ((result >> 16) & 0xFF, 10);
    EXPECT_EQ((result >> 8) & 0xFF, 20);
    EXPECT_EQ(result & 0xFF, 30);
}

TEST(MiscFunctionsTest, ConfigStringToIntRgbaZeroAlpha) {
    int64_t result = configStringToInt("rgba(255, 0, 0, 0.0)");
    EXPECT_EQ((result >> 24) & 0xFF, 0x00);
}

TEST(MiscFunctionsTest, ConfigStringToIntThrowsOnBadRgbaSyntax) {
    EXPECT_THROW(configStringToInt("rgba(123)"), std::invalid_argument);
}
