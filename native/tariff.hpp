#pragma once

#include <algorithm>
#include <cstdint>
#include <span>

namespace paw {

enum class Period { unknown, peak, offPeak };

struct TariffTransition {
  int64_t at;
  Period period;
};

struct TariffWindow {
  Period current;
  const TariffTransition* next;
};

inline TariffWindow tariffWindowAt(std::span<const TariffTransition> calendar, int64_t at) {
  const auto next = std::upper_bound(calendar.begin(), calendar.end(), at,
    [](int64_t time, const TariffTransition& transition) { return time < transition.at; });
  if (next == calendar.begin() || next == calendar.end()) return {Period::unknown, nullptr};
  return {(next - 1)->period, next->period == Period::unknown ? nullptr : &*next};
}

inline int64_t secondsUntil(int64_t now, int64_t deadline) {
  return (std::max<int64_t>(0, deadline - now) + 999) / 1000;
}

struct TariffReading {
  Period current = Period::unknown;
  const TariffTransition* next = nullptr;
  int64_t remaining = -1;
  bool operator==(const TariffReading&) const = default;
};

inline TariffReading tariffReadingAt(std::span<const TariffTransition> calendar, int64_t now) {
  const auto window = tariffWindowAt(calendar, now);
  return {window.current, window.next, window.next ? secondsUntil(now, window.next->at) : -1};
}

} // namespace paw
