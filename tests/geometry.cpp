#include "../native/geometry.hpp"
#include "../native/animation.hpp"
#include "../native/bubble.hpp"
#include "../native/protocol.hpp"
#include "../native/sounds.hpp"
#include "../native/tariff.hpp"
#include <cassert>

int main() {
  using namespace paw;
  const std::vector<Screen> screens{
    {{0, 0, 1920, 1080}, {0, 0, 1920, 1040}},
    {{-1280, -180, 0, 720}, {-1280, -180, 0, 680}},
    {{-1500, 720, -200, 1440}, {-1500, 720, -200, 1400}},
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
  const auto ready = parseBalance(fields("balance\tready\tapi\t1\tCNY\t28.60\t3.60\t25.00\tUSD\t1\t0\t1"));
  assert(ready && ready->wallets.size() == 2 && ready->wallets[1].currency == "USD");
  assert(!parseBalance(fields("balance\tready\tapi\t1")));
  const auto completed = parseTask(fields("recent\tcompleted\tmain\t4680\t4200\t480\t3200\t0.002984\t0.000448\tfull\t0\t0\t1\tapi"));
  assert(completed && completed->mode == "api" && tokenText(*completed) == L"4,680 Token");

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

  PoseAnimator animation;
  const auto seated = catPose(Action::sit, 0, 0, 0, false);
  const auto stride = catPose(Action::walk, 0, 0, 0, true);
  assert(animation.sample(seated, 0).width == seated.width);
  const auto beginning = animation.sample(stride, 1);
  assert(beginning.sprite == walkFirst && beginning.width == seated.width);
  const auto middleFrame = catPose(Action::walk, .085, 0, 5, true);
  const auto halfway = animation.sample(middleFrame, 1.085);
  assert(halfway.sprite > walkFirst && halfway.sprite < stretchFirst && halfway.width > seated.width && halfway.width < stride.width);
  assert(animation.sample(stride, 1.20).width == stride.width);
  const auto stretching = catPose(Action::stretch, 0, .675, 0, false);
  assert(stretching.sprite == spriteCount - 1);
  assert(catPose(Action::stretch, 0, stretchDuration, 0, false).sprite == stretchFirst);
  assert(spriteBounds(500, 330, stretching, 140, 160).bottom == 160);
  assert(catPose(Action::sit, 3.82, 0, 0, false).blink > .95);
  double speed = 0;
  assert(walkingStep(100, .016, 88, true, speed) < .5);
  speed = 88;
  assert(walkingStep(.5, .016, 88, true, speed) == .5 && speed < 88);

  const Rect work{0, 0, 1440, 900};
  const auto bubble = placeBubble(work, {520, 680}, 388, 444, 1, 1);
  assert(bubble.tailLeft && inBubble(bubble, bubble.frame.center()));
  assert(inBubble(bubble, {bubble.frame.left + 5, bubble.frame.top + bubble.tailY}));
  assert(!inBubble(bubble, {bubble.frame.left + 5, bubble.frame.top + bubble.tailY + 12}));
  assert(!placeBubble(work, {1350, 680}, 388, 444, 1, 1).tailLeft);
  BubbleHover hover;
  hover.opened(0);
  assert(!hover.expired(4.9, false));
  assert(!hover.expired(7, true));
  assert(!hover.expired(10, true));
  assert(!hover.expired(11, false));
  assert(!hover.expired(15.9, false));
  assert(!hover.expired(16, true));
  assert(!hover.expired(17, false));
  assert(hover.expired(22, false));
}
