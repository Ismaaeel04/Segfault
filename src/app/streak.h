// ============================================================================
// streak.h  -  Days with activity ("2026-11-10"), and streaks of them.
// No raylib here, so `streak_tests` can test it directly.
// ============================================================================
#pragma once
#include <cstdio>
#include <set>
#include <string>

namespace streak {

// Days since 1970-01-01 (Howard Hinnant's days_from_civil).
inline long DayNumber(int y, int m, int d) {
    y -= m <= 2;
    const long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long)doe - 719468;
}

// "YYYY-MM-DD" -> day number; false if it isn't a date.
inline bool Parse(const std::string& s, long& out) {
    int y, m, d;
    if (std::sscanf(s.c_str(), "%d-%d-%d", &y, &m, &d) != 3 || m < 1 || m > 12 || d < 1 || d > 31) return false;
    out = DayNumber(y, m, d);
    return true;
}

inline std::string Format(int y, int m, int d) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", y, m, d);
    return buf;
}

inline int DaysInMonth(int y, int m) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    return m == 2 && leap ? 29 : days[(m - 1) % 12];
}

// 0 = Monday ... 6 = Sunday
inline int Weekday(int y, int m, int d) {
    long n = DayNumber(y, m, d);                  // 1970-01-01 was a Thursday
    return (int)(((n % 7) + 7 + 3) % 7);
}

// Days in a row up to today. A streak is still alive if the last active day
// was yesterday (today isn't over yet).
inline int Current(const std::set<std::string>& days, const std::string& today) {
    long t;
    if (!Parse(today, t)) return 0;
    std::set<long> n;
    for (const std::string& s : days) { long v; if (Parse(s, v)) n.insert(v); }
    long d = n.count(t) ? t : n.count(t - 1) ? t - 1 : -1;
    if (d < 0) return 0;
    int run = 0;
    while (n.count(d - run)) run++;
    return run;
}

inline int Best(const std::set<std::string>& days) {
    std::set<long> n;
    for (const std::string& s : days) { long v; if (Parse(s, v)) n.insert(v); }
    int best = 0, run = 0;
    long prev = 0;
    bool first = true;
    for (long v : n) {
        run = (!first && v == prev + 1) ? run + 1 : 1;
        best = run > best ? run : best;
        prev = v;
        first = false;
    }
    return best;
}

} // namespace streak
