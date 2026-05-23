#include <gtest/gtest.h>
#include "../src/config/ConfigDataValues.hpp"

using namespace Hyprutils::Math;

ICustomConfigValueData::~ICustomConfigValueData() = default;

TEST(LayoutValueDataTest, DefaultConstruction) {
    CLayoutValueData layout;
    EXPECT_DOUBLE_EQ(layout.m_vValues.x, 0.0);
    EXPECT_DOUBLE_EQ(layout.m_vValues.y, 0.0);
    EXPECT_FALSE(layout.m_sIsRelative.x);
    EXPECT_FALSE(layout.m_sIsRelative.y);
}

TEST(LayoutValueDataTest, GetAbsolutePixels) {
    CLayoutValueData layout;
    layout.m_vValues = Vector2D(100, 200);
    layout.m_sIsRelative = {false, false};

    auto abs = layout.getAbsolute(Vector2D(1920, 1080));
    EXPECT_DOUBLE_EQ(abs.x, 100.0);
    EXPECT_DOUBLE_EQ(abs.y, 200.0);
}

TEST(LayoutValueDataTest, GetAbsolutePercent) {
    CLayoutValueData layout;
    layout.m_vValues = Vector2D(50, 25);
    layout.m_sIsRelative = {true, true};

    auto abs = layout.getAbsolute(Vector2D(1920, 1080));
    EXPECT_DOUBLE_EQ(abs.x, 960.0);
    EXPECT_DOUBLE_EQ(abs.y, 270.0);
}

TEST(LayoutValueDataTest, GetAbsoluteMixed) {
    CLayoutValueData layout;
    layout.m_vValues = Vector2D(50, 200);
    layout.m_sIsRelative = {true, false};

    auto abs = layout.getAbsolute(Vector2D(1920, 1080));
    EXPECT_DOUBLE_EQ(abs.x, 960.0);
    EXPECT_DOUBLE_EQ(abs.y, 200.0);
}

TEST(LayoutValueDataTest, GetAbsoluteZeroViewport) {
    CLayoutValueData layout;
    layout.m_vValues = Vector2D(50, 25);
    layout.m_sIsRelative = {true, true};

    auto abs = layout.getAbsolute(Vector2D(0, 0));
    EXPECT_DOUBLE_EQ(abs.x, 0.0);
    EXPECT_DOUBLE_EQ(abs.y, 0.0);
}

TEST(LayoutValueDataTest, ToStringPixels) {
    CLayoutValueData layout;
    layout.m_vValues = Vector2D(100, 200);
    layout.m_sIsRelative = {false, false};

    EXPECT_EQ(layout.toString(), "100px,200px");
}

TEST(LayoutValueDataTest, ToStringPercent) {
    CLayoutValueData layout;
    layout.m_vValues = Vector2D(50, 25);
    layout.m_sIsRelative = {true, true};

    EXPECT_EQ(layout.toString(), "50%,25%");
}

TEST(LayoutValueDataTest, ToStringMixed) {
    CLayoutValueData layout;
    layout.m_vValues = Vector2D(50, 200);
    layout.m_sIsRelative = {true, false};

    EXPECT_EQ(layout.toString(), "50%,200px");
}

TEST(LayoutValueDataTest, GetDataType) {
    CLayoutValueData layout;
    EXPECT_EQ(layout.getDataType(), CVD_TYPE_LAYOUT);
}

TEST(GradientValueDataTest, DefaultConstruction) {
    CGradientValueData grad;
    EXPECT_TRUE(grad.m_vColors.empty());
    EXPECT_TRUE(grad.m_vColorsOkLabA.empty());
    EXPECT_FLOAT_EQ(grad.m_fAngle, 0.0f);
    EXPECT_FALSE(grad.m_bIsFallback);
}

TEST(GradientValueDataTest, ColorConstruction) {
    CHyprColor col(0.5f, 0.25f, 0.75f, 1.0f);
    CGradientValueData grad(col);
    ASSERT_EQ(grad.m_vColors.size(), 1);
    EXPECT_TRUE(grad.m_vColors[0] == col);
    EXPECT_FALSE(grad.m_vColorsOkLabA.empty());
}

TEST(GradientValueDataTest, Reset) {
    CGradientValueData grad;
    CHyprColor col(1.0f, 0.0f, 0.0f, 1.0f);
    grad.reset(col);
    ASSERT_EQ(grad.m_vColors.size(), 1);
    EXPECT_TRUE(grad.m_vColors[0] == col);
    EXPECT_FLOAT_EQ(grad.m_fAngle, 0.0f);
    EXPECT_FALSE(grad.m_vColorsOkLabA.empty());
}

TEST(GradientValueDataTest, ResetClearsPrevious) {
    CGradientValueData grad(CHyprColor(1.0f, 0.0f, 0.0f, 1.0f));
    CHyprColor col2(0.0f, 1.0f, 0.0f, 1.0f);
    grad.reset(col2);
    ASSERT_EQ(grad.m_vColors.size(), 1);
    EXPECT_TRUE(grad.m_vColors[0] == col2);
}

TEST(GradientValueDataTest, UpdateColorsOk) {
    CGradientValueData grad;
    grad.m_vColors.push_back(CHyprColor(1.0f, 0.0f, 0.0f, 1.0f));
    grad.m_vColors.push_back(CHyprColor(0.0f, 0.0f, 1.0f, 1.0f));
    grad.updateColorsOk();

    EXPECT_EQ(grad.m_vColorsOkLabA.size(), 8);
}

TEST(GradientValueDataTest, EqualitySame) {
    CGradientValueData grad1(CHyprColor(1.0f, 0.0f, 0.0f, 1.0f));
    CGradientValueData grad2(CHyprColor(1.0f, 0.0f, 0.0f, 1.0f));
    EXPECT_TRUE(grad1 == grad2);
}

TEST(GradientValueDataTest, EqualityDifferentColors) {
    CGradientValueData grad1(CHyprColor(1.0f, 0.0f, 0.0f, 1.0f));
    CGradientValueData grad2(CHyprColor(0.0f, 1.0f, 0.0f, 1.0f));
    EXPECT_FALSE(grad1 == grad2);
}

TEST(GradientValueDataTest, EqualityDifferentCounts) {
    CGradientValueData grad1(CHyprColor(1.0f, 0.0f, 0.0f, 1.0f));
    CGradientValueData grad2(CHyprColor(1.0f, 0.0f, 0.0f, 1.0f));
    grad2.m_vColors.push_back(CHyprColor(0.0f, 0.0f, 1.0f, 1.0f));
    grad2.updateColorsOk();
    EXPECT_FALSE(grad1 == grad2);
}

TEST(GradientValueDataTest, EqualityDifferentAngle) {
    CGradientValueData grad1(CHyprColor(1.0f, 0.0f, 0.0f, 1.0f));
    CGradientValueData grad2(CHyprColor(1.0f, 0.0f, 0.0f, 1.0f));
    grad2.m_fAngle = 90.0f * M_PI / 180.0f;
    EXPECT_FALSE(grad1 == grad2);
}

TEST(GradientValueDataTest, ToStringSingleColor) {
    CGradientValueData grad(CHyprColor(1.0f, 0.0f, 0.0f, 1.0f));
    std::string str = grad.toString();
    EXPECT_TRUE(str.find("deg") != std::string::npos);
    EXPECT_TRUE(str.find("ff0000") != std::string::npos || str.find("FF0000") != std::string::npos);
}

TEST(GradientValueDataTest, GetDataType) {
    CGradientValueData grad;
    EXPECT_EQ(grad.getDataType(), CVD_TYPE_GRADIENT);
}

TEST(GradientValueDataTest, AngleInToString) {
    CGradientValueData grad(CHyprColor(1.0f, 0.0f, 0.0f, 1.0f));
    grad.m_fAngle = M_PI;
    std::string str = grad.toString();
    EXPECT_TRUE(str.find("180deg") != std::string::npos);
}

TEST(GradientValueDataTest, FallbackFlag) {
    CGradientValueData grad;
    grad.m_bIsFallback = true;
    EXPECT_TRUE(grad.m_bIsFallback);
    grad.m_bIsFallback = false;
    EXPECT_FALSE(grad.m_bIsFallback);
}

TEST(GradientValueDataTest, MultipleColorsOkLab) {
    CGradientValueData grad;
    grad.m_vColors.push_back(CHyprColor(1.0f, 0.0f, 0.0f, 1.0f));
    grad.m_vColors.push_back(CHyprColor(0.0f, 1.0f, 0.0f, 1.0f));
    grad.m_vColors.push_back(CHyprColor(0.0f, 0.0f, 1.0f, 1.0f));
    grad.updateColorsOk();

    EXPECT_EQ(grad.m_vColorsOkLabA.size(), 12);
    EXPECT_NEAR(grad.m_vColorsOkLabA[0], 0.6279, 0.01);
    EXPECT_NEAR(grad.m_vColorsOkLabA[4], 0.8643, 0.01);
    EXPECT_NEAR(grad.m_vColorsOkLabA[8], 0.4520, 0.01);
}
