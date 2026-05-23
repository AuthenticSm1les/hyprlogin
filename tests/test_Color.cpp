#include <gtest/gtest.h>
#include "../src/helpers/Color.hpp"

TEST(ColorTest, DefaultConstructor) {
    CHyprColor c;
    EXPECT_DOUBLE_EQ(c.r, 0.0);
    EXPECT_DOUBLE_EQ(c.g, 0.0);
    EXPECT_DOUBLE_EQ(c.b, 0.0);
    EXPECT_DOUBLE_EQ(c.a, 0.0);
}

TEST(ColorTest, FloatConstructor) {
    CHyprColor c(0.5f, 0.25f, 0.75f, 1.0f);
    EXPECT_DOUBLE_EQ(c.r, 0.5);
    EXPECT_DOUBLE_EQ(c.g, 0.25);
    EXPECT_DOUBLE_EQ(c.b, 0.75);
    EXPECT_DOUBLE_EQ(c.a, 1.0);
}

TEST(ColorTest, HexConstructor) {
    CHyprColor c(0xFFFF0000ULL);
    EXPECT_DOUBLE_EQ(c.a, 1.0);
    EXPECT_DOUBLE_EQ(c.r, 1.0);
    EXPECT_DOUBLE_EQ(c.g, 0.0);
    EXPECT_DOUBLE_EQ(c.b, 0.0);
}

TEST(ColorTest, HexConstructorSemiTransparent) {
    CHyprColor c(0x80FF0000ULL);
    EXPECT_NEAR(c.a, 0.50196, 0.001);
    EXPECT_DOUBLE_EQ(c.r, 1.0);
    EXPECT_DOUBLE_EQ(c.g, 0.0);
    EXPECT_DOUBLE_EQ(c.b, 0.0);
}

TEST(ColorTest, HexConstructorGreen) {
    CHyprColor c(0xFF00FF00ULL);
    EXPECT_DOUBLE_EQ(c.a, 1.0);
    EXPECT_DOUBLE_EQ(c.r, 0.0);
    EXPECT_DOUBLE_EQ(c.g, 1.0);
    EXPECT_DOUBLE_EQ(c.b, 0.0);
}

TEST(ColorTest, HexConstructorBlue) {
    CHyprColor c(0xFF0000FFULL);
    EXPECT_DOUBLE_EQ(c.a, 1.0);
    EXPECT_DOUBLE_EQ(c.r, 0.0);
    EXPECT_DOUBLE_EQ(c.g, 0.0);
    EXPECT_DOUBLE_EQ(c.b, 1.0);
}

TEST(ColorTest, HexConstructorWhite) {
    CHyprColor c(0xFFFFFFFFULL);
    EXPECT_DOUBLE_EQ(c.a, 1.0);
    EXPECT_DOUBLE_EQ(c.r, 1.0);
    EXPECT_DOUBLE_EQ(c.g, 1.0);
    EXPECT_DOUBLE_EQ(c.b, 1.0);
}

TEST(ColorTest, HexConstructorBlack) {
    CHyprColor c(0xFF000000ULL);
    EXPECT_DOUBLE_EQ(c.a, 1.0);
    EXPECT_DOUBLE_EQ(c.r, 0.0);
    EXPECT_DOUBLE_EQ(c.g, 0.0);
    EXPECT_DOUBLE_EQ(c.b, 0.0);
}

TEST(ColorTest, GetAsHex) {
    CHyprColor c(1.0f, 0.0f, 0.0f, 1.0f);
    EXPECT_EQ(c.getAsHex(), 0xFFFF0000U);

    CHyprColor c2(0.0f, 1.0f, 0.0f, 1.0f);
    EXPECT_EQ(c2.getAsHex(), 0xFF00FF00U);

    CHyprColor c3(0.0f, 0.0f, 1.0f, 1.0f);
    EXPECT_EQ(c3.getAsHex(), 0xFF0000FFU);
}

TEST(ColorTest, GetAsHexSemiTransparent) {
    CHyprColor c(0.5f, 0.25f, 0.75f, 0.5f);
    const auto hex = c.getAsHex();
    EXPECT_EQ((hex >> 24) & 0xFF, 0x7F);
    EXPECT_EQ((hex >> 16) & 0xFF, 0x7F);
    EXPECT_EQ((hex >> 8) & 0xFF, 0x3F);
    EXPECT_EQ(hex & 0xFF, 0xBF);
}

TEST(ColorTest, AsRGB) {
    CHyprColor c(0.3f, 0.6f, 0.9f, 1.0f);
    auto rgb = c.asRGB();
    EXPECT_FLOAT_EQ(rgb.r, 0.3f);
    EXPECT_FLOAT_EQ(rgb.g, 0.6f);
    EXPECT_FLOAT_EQ(rgb.b, 0.9f);
}

TEST(ColorTest, StripAlpha) {
    CHyprColor c(0.3f, 0.6f, 0.9f, 0.5f);
    auto stripped = c.stripA();
    EXPECT_FLOAT_EQ(stripped.r, 0.3f);
    EXPECT_FLOAT_EQ(stripped.g, 0.6f);
    EXPECT_FLOAT_EQ(stripped.b, 0.9f);
    EXPECT_DOUBLE_EQ(stripped.a, 1.0);
}

TEST(ColorTest, EqualityOperator) {
    CHyprColor a(0.5f, 0.25f, 0.75f, 1.0f);
    CHyprColor b(0.5f, 0.25f, 0.75f, 1.0f);
    CHyprColor c(0.1f, 0.2f, 0.3f, 0.4f);

    EXPECT_TRUE(a == b);
    EXPECT_TRUE(b == a);
    EXPECT_FALSE(a == c);
    EXPECT_FALSE(c == b);
}

TEST(ColorTest, AsOkLab) {
    CHyprColor c(1.0f, 0.0f, 0.0f, 1.0f);
    auto lab = c.asOkLab();
    EXPECT_NEAR(lab.l, 0.6279, 0.01);
}

TEST(ColorTest, AsHSL) {
    CHyprColor c(1.0f, 0.0f, 0.0f, 1.0f);
    auto hsl = c.asHSL();
    EXPECT_NEAR(hsl.h, 0.0, 1.0);
    EXPECT_NEAR(hsl.s, 1.0, 0.01);
    EXPECT_NEAR(hsl.l, 0.5, 0.01);
}

TEST(ColorTest, HexConstructorRoundtrip) {
    uint64_t original = 0xAB123456ULL;
    CHyprColor c(original);
    EXPECT_EQ(c.getAsHex(), static_cast<uint32_t>(original & 0xFFFFFFFFULL));
}

TEST(ColorTest, ConstructFromHyprgraphicsColor) {
    Hyprgraphics::CColor hgCol(Hyprgraphics::CColor::SSRGB{.r = 0.2, .g = 0.4, .b = 0.6});
    CHyprColor c(hgCol, 0.8f);
    EXPECT_FLOAT_EQ(c.a, 0.8f);
    EXPECT_FLOAT_EQ(c.r, 0.2f);
    EXPECT_FLOAT_EQ(c.g, 0.4f);
    EXPECT_FLOAT_EQ(c.b, 0.6f);
}

TEST(ColorTest, OkLabCacheConsistency) {
    CHyprColor c1(1.0f, 0.0f, 0.0f, 1.0f);
    auto lab1 = c1.asOkLab();
    auto lab2 = c1.asOkLab();
    EXPECT_DOUBLE_EQ(lab1.l, lab2.l);
    EXPECT_DOUBLE_EQ(lab1.a, lab2.a);
    EXPECT_DOUBLE_EQ(lab1.b, lab2.b);
}

TEST(ColorTest, HSVChromaLimits) {
    CHyprColor c(0.0f, 1.0f, 0.0f, 0.5f);
    EXPECT_DOUBLE_EQ(c.r, 0.0);
    EXPECT_DOUBLE_EQ(c.g, 1.0);
    EXPECT_DOUBLE_EQ(c.b, 0.0);
    EXPECT_DOUBLE_EQ(c.a, 0.5);
}

TEST(ColorTest, HexParsesAlphaCorrectly) {
    CHyprColor c(0x00000000ULL);
    EXPECT_DOUBLE_EQ(c.a, 0.0);
    EXPECT_DOUBLE_EQ(c.r, 0.0);
    EXPECT_DOUBLE_EQ(c.g, 0.0);
    EXPECT_DOUBLE_EQ(c.b, 0.0);
}
