#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <queue>
#include <vector>

namespace paw {

struct Point {
  double x{}, y{};
  Point operator+(Point p) const { return {x + p.x, y + p.y}; }
  Point operator-(Point p) const { return {x - p.x, y - p.y}; }
  Point operator*(double factor) const { return {x * factor, y * factor}; }
};

inline double length(Point p) { return std::hypot(p.x, p.y); }

struct Rect {
  double left{}, top{}, right{}, bottom{};
  double width() const { return right - left; }
  double height() const { return bottom - top; }
  bool contains(Point p) const { return p.x >= left && p.x < right && p.y >= top && p.y < bottom; }
  Point center() const { return {(left + right) / 2, (top + bottom) / 2}; }
  Point clamp(Point p) const {
    return {std::clamp(p.x, left, std::max(left, right - 1)), std::clamp(p.y, top, std::max(top, bottom - 1))};
  }
};

struct Screen { Rect bounds, work; };

inline size_t nearestScreen(const std::vector<Screen>& screens, Point p) {
  size_t nearest = 0;
  double distance = std::numeric_limits<double>::infinity();
  for (size_t i = 0; i < screens.size(); ++i) {
    if (screens[i].bounds.contains(p)) return i;
    const double candidate = length(p - screens[i].bounds.clamp(p));
    if (candidate < distance) { distance = candidate; nearest = i; }
  }
  return nearest;
}

inline Rect safeArea(Rect work, double scale) {
  const double x = std::min(110 * scale, work.width() / 3);
  const double top = std::min(168 * scale, work.height() / 2);
  const double bottom = std::min(23 * scale, work.height() / 4);
  return {work.left + x, work.top + top, work.right - x, work.bottom - bottom};
}

struct Gate { Point leave, enter; };

inline std::optional<Gate> sharedGate(Rect a, Rect b) {
  const double top = std::max(a.top, b.top), bottom = std::min(a.bottom, b.bottom);
  const double left = std::max(a.left, b.left), right = std::min(a.right, b.right);
  if (bottom > top && a.right == b.left) return Gate{{a.right - 1, (top + bottom) / 2}, {b.left, (top + bottom) / 2}};
  if (bottom > top && b.right == a.left) return Gate{{a.left, (top + bottom) / 2}, {b.right - 1, (top + bottom) / 2}};
  if (right > left && a.bottom == b.top) return Gate{{(left + right) / 2, a.bottom - 1}, {(left + right) / 2, b.top}};
  if (right > left && b.bottom == a.top) return Gate{{(left + right) / 2, a.top}, {(left + right) / 2, b.bottom - 1}};
  if (right > left && bottom > top) {
    const Point center{(left + right) / 2, (top + bottom) / 2};
    return Gate{center, center};
  }
  return std::nullopt;
}

inline std::vector<Point> route(const std::vector<Screen>& screens, size_t from, size_t to, Point goal) {
  if (from == to) return {goal};
  std::vector<size_t> parents(screens.size(), screens.size());
  std::queue<size_t> search;
  search.push(from);
  parents[from] = from;
  while (!search.empty() && parents[to] == screens.size()) {
    const size_t at = search.front();
    search.pop();
    for (size_t next = 0; next < screens.size(); ++next) {
      if (parents[next] != screens.size() || !sharedGate(screens[at].bounds, screens[next].bounds)) continue;
      parents[next] = at;
      search.push(next);
    }
  }
  if (parents[to] == screens.size()) return {};
  std::vector<size_t> chain;
  for (size_t at = to; at != from; at = parents[at]) chain.push_back(at);
  std::vector<Point> result;
  size_t at = from;
  for (auto iter = chain.rbegin(); iter != chain.rend(); ++iter) {
    const Gate gate = *sharedGate(screens[at].bounds, screens[*iter].bounds);
    result.push_back(gate.leave);
    result.push_back(gate.enter);
    at = *iter;
  }
  result.push_back(goal);
  return result;
}

} // namespace paw
