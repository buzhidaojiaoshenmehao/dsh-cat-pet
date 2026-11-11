#include "../windows/geometry.hpp"
#include "../windows/protocol.hpp"
#include "../windows/sounds.hpp"
#include "../windows/tariff.hpp"
#include <cassert>

int main() {
  using namespace paw;
  const std::vector<Screen> screens{
    {{0, 0, 1920, 1080}, {0, 0, 1920, 1040}},
    {{-1280, -180, 0, 720}, {-1280, -180, 0, 680}},
    {{-1280, 720, 0, 1440}, {-1280, 720, 0, 1400}},
    {{3100, 0, 4100, 900}, {3100, 0, 4100, 860}},
  };
  assert(nearestScreen(screens, {-430, -100}) == 1);
  const auto steps = route(screens, 0, 2, {-900, 1000});
  assert(steps.size() == 5);
  for (Point point : steps) {
    assert(std::any_of(screens.begin(), screens.end(), [point](const Screen& s) { return s.bounds.contains(point); }));
  }
  assert(route(screens, 0, 3, {3400, 350}).empty());
  const Rect visible = safeArea(screens[1].work, 1.5);
  assert(visible.contains(visible.clamp({-9999, -9999})));
  const Task task{"completed", "main", "4680", "4200", "480", "3200", "0.002984", "0.000448", "full", "api"};
  const Balance balance{"ready", "api", "1", {{"USD", "10", "0", "10"}}};
  assert(grouped(task.total) == L"4,680");
  assert(taskPrice(task, balance) == L"预估 $0.000448");
  assert(fields("activity\tthinking\t2").size() == 3);

  const TariffTransition calendar[]{
    {0, Period::offPeak}, {4000, Period::peak}, {6000, Period::offPeak}, {8000, Period::unknown},
  };
  assert(tariffWindowAt(calendar, -1).current == Period::unknown);
  const auto beforePeak = tariffWindowAt(calendar, 3999);
  assert(beforePeak.current == Period::offPeak && beforePeak.next->at == 4000);
  assert(secondsUntil(3999, beforePeak.next->at) == 1);
  assert(tariffWindowAt(calendar, 4000).current == Period::peak);
  assert(tariffWindowAt(calendar, 4000).next->at == 6000);
  assert(tariffWindowAt(calendar, 7000).next == nullptr);
  assert(tariffWindowAt(calendar, 8000).current == Period::unknown);

  const auto wave = makeChime({{660, 68}, {880, 115}});
  assert(wave.size() == 44 + 24 * (68 + 115) * 2);
  assert(std::string_view(reinterpret_cast<const char*>(wave.data()), 4) == "RIFF");
  assert(std::string_view(reinterpret_cast<const char*>(wave.data() + 36), 4) == "data");
}
