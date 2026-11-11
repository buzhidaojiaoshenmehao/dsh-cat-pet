#pragma once

#include "geometry.hpp"
#include <array>
#include <numbers>
#include <span>

namespace paw {

enum SpriteId {
  idle, curious, pawUp, sleepPose, belly, crouch,
  walk1, walk1a, walk1b, walk2, walk2a, walk2b, walk3, walk3a, walk3b, walk4, walk4a, walk4b, spriteCount
};
enum class Action { sit, look, sleep, work, walk, reach, roll, stretch, celebrate, notice };

struct Pose {
  SpriteId sprite = idle;
  double width = 158, height = 142, lift = 0;
  bool mirrored = false;
  double stretchX = 1, stretchY = 1, shiftX = 0, blink = 0;
};

inline double smoothStep(double value) {
  const double t = std::clamp(value, 0.0, 1.0);
  return t * t * (3 - 2 * t);
}

inline double pulse(double age, double duration) {
  if (age <= 0 || age >= duration) return 0;
  return std::sin(std::numbers::pi * age / duration);
}

inline Pose catPose(Action action, double seconds, double age, double distance, bool facingRight) {
  const double breath = std::sin(seconds * 2.5);
  switch (action) {
    case Action::walk: {
      const double step = distance / 13;
      const int frame = int(std::fmod(step * 3, spriteCount - walk1));
      return {SpriteId(walk1 + frame), 207, 119,
        -2.6 * std::abs(std::sin(step * std::numbers::pi / 2)), facingRight,
        1 + .016 * std::cos(step * std::numbers::pi), 1, facingRight ? 2.0 : -2.0};
    }
    case Action::reach: {
      const double paw = pulse(age, .52);
      return {pawUp, 158, 144, -8 * paw, facingRight, 1 + .05 * paw, 1 - .025 * paw, (facingRight ? 9 : -9) * paw};
    }
    case Action::roll:
      return {belly, 188, 114, -3 * pulse(age, .7), false,
        1 + .04 * pulse(age, 1.5), 1 + .022 * breath, 4 * std::sin(age * 8) * pulse(age, 1.5)};
    case Action::stretch: {
      const double extension = pulse(age, 1.35);
      return {crouch, 179, 104, 0, facingRight, 1 + .17 * extension, 1 - .16 * extension,
        (facingRight ? 9 : -9) * extension};
    }
    case Action::celebrate:
      return {belly, 188, 126, -14 * pulse(age, .62), false, 1, 1 + .025 * breath};
    case Action::notice:
      return {curious, 158, 142, -3 * pulse(age, .6)};
    case Action::sleep:
      return {sleepPose, 173, 97, 0, false, 1 + .009 * breath, 1 + .028 * std::sin(seconds * 1.9)};
    case Action::work:
      return {crouch, 179, 104, 0, facingRight, 1 + .012 * breath, 1 + .018 * breath};
    case Action::look:
    case Action::sit: {
      const double cycle = std::fmod(seconds + 1.7, 5.4);
      const double blink = std::max(pulse(cycle, .24), pulse(cycle - .38, .18));
      return {action == Action::look ? curious : idle, 158, 142, 0, false,
        1 + .008 * breath, 1 + .016 * breath, 0, blink};
    }
  }
  return {};
}

class PoseAnimator {
public:
  Pose sample(Pose pose, double now) {
    if (!ready_) {
      ready_ = true;
      last_ = pose;
      target_ = pose;
    }
    const bool sameGait = pose.sprite >= walk1 && target_.sprite >= walk1 && pose.mirrored == target_.mirrored;
    if (!sameGait && (pose.sprite != target_.sprite || pose.mirrored != target_.mirrored)) {
      from_ = last_;
      changed_ = now;
      transitioning_ = true;
    }
    target_ = pose;
    if (transitioning_) {
      const double mix = smoothStep((now - changed_) / .17);
      pose.width = std::lerp(from_.width, pose.width, mix);
      pose.height = std::lerp(from_.height, pose.height, mix);
      pose.lift = std::lerp(from_.lift, pose.lift, mix);
      pose.shiftX = std::lerp(from_.shiftX, pose.shiftX, mix);
      pose.stretchX = std::lerp(from_.stretchX, pose.stretchX, mix);
      pose.stretchY = std::lerp(from_.stretchY, pose.stretchY, mix);
      transitioning_ = mix < 1;
    }
    last_ = pose;
    return pose;
  }

private:
  bool ready_ = false, transitioning_ = false;
  Pose from_, last_, target_;
  double changed_ = 0;
};

inline Rect spriteBounds(double imageWidth, double imageHeight, const Pose& pose, double center, double feet) {
  const double fit = std::min(pose.width / imageWidth, pose.height / imageHeight);
  const double width = imageWidth * fit * pose.stretchX, height = imageHeight * fit * pose.stretchY;
  const double x = center + pose.shiftX - width / 2, bottom = feet + pose.lift;
  return {x, bottom - height, x + width, bottom};
}

struct Eye { double x, y, radiusX, radiusY; };

inline std::span<const Eye> eyesFor(SpriteId sprite) {
  // Eye centers and eyelid bounds measured in the two transparent sprites.
  static constexpr std::array<Eye, 2> sitting{{{129.0/424, 113.0/472, 15.0/424, 13.0/472},
    {197.0/424, 120.0/472, 15.0/424, 13.0/472}}};
  static constexpr std::array<Eye, 2> looking{{{127.0/415, 114.0/474, 13.0/415, 13.0/474},
    {186.0/415, 137.0/474, 15.0/415, 12.0/474}}};
  if (sprite == idle) return sitting;
  if (sprite == curious) return looking;
  return {};
}

inline double walkingStep(double remaining, double dt, double limit, bool arriving, double& speed) {
  const double target = arriving ? std::min(limit, std::max(limit * .20, remaining * 4.0)) : limit;
  speed += (target - speed) * (1 - std::exp(-dt * 10));
  return std::min(remaining, speed * dt);
}

} // namespace paw
