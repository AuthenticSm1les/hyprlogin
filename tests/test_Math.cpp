#include <gtest/gtest.h>
#include "../src/helpers/Math.hpp"

TEST(MathTest, WlTransformToHyprutilsNormal) {
    EXPECT_EQ(wlTransformToHyprutils(WL_OUTPUT_TRANSFORM_NORMAL), Hyprutils::Math::eTransform::HYPRUTILS_TRANSFORM_NORMAL);
}

TEST(MathTest, WlTransformToHyprutils90) {
    EXPECT_EQ(wlTransformToHyprutils(WL_OUTPUT_TRANSFORM_90), Hyprutils::Math::eTransform::HYPRUTILS_TRANSFORM_90);
}

TEST(MathTest, WlTransformToHyprutils180) {
    EXPECT_EQ(wlTransformToHyprutils(WL_OUTPUT_TRANSFORM_180), Hyprutils::Math::eTransform::HYPRUTILS_TRANSFORM_180);
}

TEST(MathTest, WlTransformToHyprutils270) {
    EXPECT_EQ(wlTransformToHyprutils(WL_OUTPUT_TRANSFORM_270), Hyprutils::Math::eTransform::HYPRUTILS_TRANSFORM_270);
}

TEST(MathTest, WlTransformToHyprutilsFlipped) {
    EXPECT_EQ(wlTransformToHyprutils(WL_OUTPUT_TRANSFORM_FLIPPED), Hyprutils::Math::eTransform::HYPRUTILS_TRANSFORM_FLIPPED);
}

TEST(MathTest, WlTransformToHyprutilsFlipped180) {
    EXPECT_EQ(wlTransformToHyprutils(WL_OUTPUT_TRANSFORM_FLIPPED_180), Hyprutils::Math::eTransform::HYPRUTILS_TRANSFORM_FLIPPED_180);
}

TEST(MathTest, WlTransformToHyprutilsFlipped270) {
    EXPECT_EQ(wlTransformToHyprutils(WL_OUTPUT_TRANSFORM_FLIPPED_270), Hyprutils::Math::eTransform::HYPRUTILS_TRANSFORM_FLIPPED_270);
}

TEST(MathTest, WlTransformToHyprutilsFlipped90) {
    EXPECT_EQ(wlTransformToHyprutils(WL_OUTPUT_TRANSFORM_FLIPPED_90), Hyprutils::Math::eTransform::HYPRUTILS_TRANSFORM_FLIPPED_90);
}

TEST(MathTest, WlTransformToHyprutilsInvalidDefaultsToNormal) {
    EXPECT_EQ(wlTransformToHyprutils(static_cast<wl_output_transform>(999)), Hyprutils::Math::eTransform::HYPRUTILS_TRANSFORM_NORMAL);
}

TEST(MathTest, InvertTransformNormal) {
    EXPECT_EQ(invertTransform(WL_OUTPUT_TRANSFORM_NORMAL), WL_OUTPUT_TRANSFORM_NORMAL);
}

TEST(MathTest, InvertTransform180) {
    EXPECT_EQ(invertTransform(WL_OUTPUT_TRANSFORM_180), WL_OUTPUT_TRANSFORM_180);
}

TEST(MathTest, InvertTransform90) {
    EXPECT_EQ(invertTransform(WL_OUTPUT_TRANSFORM_90), WL_OUTPUT_TRANSFORM_270);
}

TEST(MathTest, InvertTransform270) {
    EXPECT_EQ(invertTransform(WL_OUTPUT_TRANSFORM_270), WL_OUTPUT_TRANSFORM_90);
}

TEST(MathTest, InvertTransformFlipped) {
    EXPECT_EQ(invertTransform(WL_OUTPUT_TRANSFORM_FLIPPED), WL_OUTPUT_TRANSFORM_FLIPPED);
}

TEST(MathTest, InvertTransformFlipped90) {
    EXPECT_EQ(invertTransform(WL_OUTPUT_TRANSFORM_FLIPPED_90), WL_OUTPUT_TRANSFORM_FLIPPED_90);
}

TEST(MathTest, InvertTransformFlipped180) {
    EXPECT_EQ(invertTransform(WL_OUTPUT_TRANSFORM_FLIPPED_180), WL_OUTPUT_TRANSFORM_FLIPPED_180);
}

TEST(MathTest, InvertTransformFlipped270) {
    EXPECT_EQ(invertTransform(WL_OUTPUT_TRANSFORM_FLIPPED_270), WL_OUTPUT_TRANSFORM_FLIPPED_270);
}

TEST(MathTest, InvertTransformIdentityCheck) {
    for (int t = WL_OUTPUT_TRANSFORM_NORMAL; t <= WL_OUTPUT_TRANSFORM_FLIPPED_270; ++t) {
        auto tr = static_cast<wl_output_transform>(t);
        auto inverted = invertTransform(tr);
        auto doubleInverted = invertTransform(inverted);
        EXPECT_EQ(doubleInverted, tr);
    }
}
