// ============================================================================
// test_streak.cpp  -  Console unit tests for activity days and streaks.
// Build target: streak_tests   (run it, or use `ctest` in the build folder)
// ============================================================================
#include <cstdio>
#include "../src/app/streak.h"

using namespace streak;

static int passed = 0, failed = 0;

#define CHECK(cond) do { if (cond) passed++; else { failed++; \
    printf("  FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

int main() {
    printf("[dates]\n");
    CHECK(DayNumber(1970, 1, 1) == 0);
    CHECK(DayNumber(2026, 11, 10) - DayNumber(2026, 10, 31) == 10);
    CHECK(DayNumber(2024, 3, 1) - DayNumber(2024, 2, 28) == 2);   // leap year
    CHECK(DayNumber(2026, 3, 1) - DayNumber(2026, 2, 28) == 1);
    CHECK(DaysInMonth(2024, 2) == 29 && DaysInMonth(2026, 2) == 28 && DaysInMonth(2000, 2) == 29 && DaysInMonth(1900, 2) == 28);
    CHECK(DaysInMonth(2026, 11) == 30 && DaysInMonth(2026, 12) == 31);
    CHECK(Weekday(2026, 11, 10) == 1);   // a Tuesday
    CHECK(Weekday(2026, 11, 1) == 6);    // a Sunday
    CHECK(Format(2026, 1, 5) == "2026-01-05");
    long v;
    CHECK(Parse("2026-11-10", v) && v == DayNumber(2026, 11, 10));
    CHECK(!Parse("garbage", v) && !Parse("2026-13-01", v));

    printf("[streaks]\n");
    std::set<std::string> days = {"2026-11-06", "2026-11-07", "2026-11-08", "2026-11-09"};
    CHECK(Current(days, "2026-11-09") == 4);
    CHECK(Current(days, "2026-11-10") == 4);   // nothing today yet: still alive
    CHECK(Current(days, "2026-11-11") == 0);   // a day missed
    days.insert("2026-11-10");
    CHECK(Current(days, "2026-11-10") == 5);
    CHECK(Best(days) == 5);
    std::set<std::string> gap = {"2026-10-30", "2026-10-31", "2026-11-01", "2026-11-05", "2026-11-06"};
    CHECK(Best(gap) == 3);                     // across the end of the month
    CHECK(Current(gap, "2026-11-06") == 2);
    CHECK(Best({}) == 0 && Current({}, "2026-11-10") == 0);
    CHECK(Best({"2026-11-10", "not a date"}) == 1);

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
