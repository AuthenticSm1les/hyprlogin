#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "../src/core/Timer.hpp"

static void nullCallback(ASP<CTimer> self, void* data) {}

TEST(TimerTest, Construction) {
    CTimer timer(std::chrono::seconds(60), nullCallback, nullptr, false);
    EXPECT_FALSE(timer.passed());
    EXPECT_FALSE(timer.cancelled());
    EXPECT_FALSE(timer.canForceUpdate());
}

TEST(TimerTest, ConstructionWithForce) {
    CTimer timer(std::chrono::seconds(60), nullCallback, nullptr, true);
    EXPECT_TRUE(timer.canForceUpdate());
}

TEST(TimerTest, PassedReturnsTrueForZeroTimeout) {
    CTimer timer(std::chrono::seconds(0), nullCallback, nullptr, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_TRUE(timer.passed());
}

TEST(TimerTest, PassedReturnsFalseForFutureTimeout) {
    CTimer timer(std::chrono::hours(24), nullCallback, nullptr, false);
    EXPECT_FALSE(timer.passed());
}

TEST(TimerTest, CancelSetsCancelled) {
    CTimer timer(std::chrono::seconds(60), nullCallback, nullptr, false);
    EXPECT_FALSE(timer.cancelled());
    timer.cancel();
    EXPECT_TRUE(timer.cancelled());
}

TEST(TimerTest, LeftMsPositive) {
    CTimer timer(std::chrono::hours(1), nullCallback, nullptr, false);
    EXPECT_GT(timer.leftMs(), 0);
}

TEST(TimerTest, LeftMsNearZero) {
    CTimer timer(std::chrono::milliseconds(1), nullCallback, nullptr, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EXPECT_LE(timer.leftMs(), 0);
}

TEST(TimerTest, CallbackInvocation) {
    bool called = false;
    auto cb = [&called](ASP<CTimer> self, void* data) {
        called = true;
    };

    CTimer timer(std::chrono::seconds(60), cb, nullptr, false);
    EXPECT_FALSE(called);
    timer.call(ASP<CTimer>{});
    EXPECT_TRUE(called);
}

TEST(TimerTest, CallbackReceivesData) {
    int receivedData = 0;
    int expectedData = 42;
    auto cb = [&receivedData](ASP<CTimer> self, void* data) {
        receivedData = *static_cast<int*>(data);
    };

    CTimer timer(std::chrono::seconds(60), cb, &expectedData, false);
    timer.call(ASP<CTimer>{});
    EXPECT_EQ(receivedData, expectedData);
}

TEST(TimerTest, PassesAfterTimeout) {
    CTimer timer(std::chrono::milliseconds(1), nullCallback, nullptr, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_TRUE(timer.passed());
}

TEST(TimerTest, CancelledTimerDoesNotPass) {
    CTimer timer(std::chrono::milliseconds(1), nullCallback, nullptr, false);
    timer.cancel();
    EXPECT_TRUE(timer.cancelled());
}

TEST(TimerTest, MultipleCancelCalls) {
    CTimer timer(std::chrono::seconds(60), nullCallback, nullptr, false);
    timer.cancel();
    timer.cancel();
    timer.cancel();
    EXPECT_TRUE(timer.cancelled());
}

TEST(TimerTest, DifferentTimeouts) {
    CTimer short_timer(std::chrono::milliseconds(1), nullCallback, nullptr, false);
    CTimer long_timer(std::chrono::hours(24), nullCallback, nullptr, false);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    EXPECT_TRUE(short_timer.passed());
    EXPECT_FALSE(long_timer.passed());
}
