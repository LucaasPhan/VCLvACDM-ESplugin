#include <gtest/gtest.h>

#include "types/Pilot.h"
#include "utils/Date.h"

using namespace vacdm::utils;

namespace utils::tests {
TEST(DateUtilTest, ParsesBackendIsoTimestampsWithMilliseconds) {
    const auto withoutMilliseconds = Date::isoStringToTimestamp("2026-06-02T10:20:00Z");
    const auto withMilliseconds = Date::isoStringToTimestamp("2026-06-02T10:20:00.000Z");

    EXPECT_NE(withoutMilliseconds, vacdm::types::defaultTime);
    EXPECT_EQ(withMilliseconds, withoutMilliseconds);
}

TEST(DateUtilTest, TreatsNullAndEmptyIsoTimestampsAsDefaultTime) {
    EXPECT_EQ(Date::isoStringToTimestamp(""), vacdm::types::defaultTime);
    EXPECT_EQ(Date::isoStringToTimestamp("null"), vacdm::types::defaultTime);
}
}  // namespace utils::tests
