#pragma once

#include "geometry.hpp"
#include <cstdint>

namespace paw {

inline constexpr double bubbleGutter = 14, bubbleRadius = 16, bubbleTailHalfHeight = 10;
inline constexpr double bubbleWidth = 300, bubbleHeight = 328, bubbleDetailsHeight = 56;
inline constexpr uint32_t bubbleFill = 0xfaf4ed, bubbleControlsFill = 0xeceee5, bubbleDivider = 0xe0d8cd;
inline constexpr uint32_t bubblePeak = 0xa62d33, bubblePeakFill = 0xf7deda;
inline constexpr uint32_t bubbleOffPeak = 0x216344, bubbleOffPeakFill = 0xddefe3;

struct BubbleLayout {
  Rect frame;
  double scale = 1;
  bool tailLeft = true;
  double tailY = 0;
};

inline BubbleLayout placeBubble(Rect work, Point cat, double width, double height, double scale, double petScale) {
  const double totalWidth = (width + 2 * bubbleGutter) * scale;
  const double totalHeight = height * scale;
  const double gap = 88 * petScale;
  const double right = work.right - cat.x - gap, left = cat.x - gap - work.left;
  const bool tailLeft = right >= totalWidth || right >= left;
  const double candidate = tailLeft ? cat.x + gap : cat.x - gap - totalWidth;
  const double x = std::clamp(candidate, work.left, std::max(work.left, work.right - totalWidth));
  const double face = cat.y - 66 * petScale;
  const double top = std::clamp(face - totalHeight * .63, work.top, std::max(work.top, work.bottom - totalHeight));
  const double margin = (bubbleRadius + bubbleTailHalfHeight + 2) * scale;
  const double tailY = std::clamp(face - top, margin, totalHeight - margin);
  return {{x, top, x + totalWidth, top + totalHeight}, scale, tailLeft, tailY};
}

inline bool inBubble(const BubbleLayout& bubble, Point screen) {
  if (!bubble.frame.contains(screen)) return false;
  const double x = screen.x - bubble.frame.left, y = screen.y - bubble.frame.top;
  const double width = bubble.frame.width(), height = bubble.frame.height();
  const double gutter = bubbleGutter * bubble.scale, radius = bubbleRadius * bubble.scale;
  if (x >= gutter && x <= width - gutter) {
    const double innerX = std::clamp(x, gutter + radius, width - gutter - radius);
    const double innerY = std::clamp(y, radius, height - radius);
    return std::hypot(x - innerX, y - innerY) <= radius;
  }
  const double fromTip = bubble.tailLeft ? x : width - x;
  return fromTip <= gutter && std::abs(y - bubble.tailY) <= bubbleTailHalfHeight * bubble.scale * fromTip / gutter;
}

class BubbleHover {
public:
  void opened(double now) { leftAt_ = now; }

  bool expired(double now, bool inside) {
    if (inside) {
      leftAt_.reset();
      return false;
    }
    if (!leftAt_) leftAt_ = now;
    return now - *leftAt_ >= 5;
  }

private:
  std::optional<double> leftAt_;
};

} // namespace paw
