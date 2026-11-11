#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <numbers>
#include <vector>

namespace paw {

struct Note {
  double hz;
  unsigned milliseconds;
};

inline std::vector<uint8_t> makeChime(std::initializer_list<Note> notes) {
  constexpr unsigned sampleRate = 24000;
  unsigned samples = 0;
  for (const Note note : notes) samples += note.milliseconds * sampleRate / 1000;
  const unsigned dataBytes = samples * 2;
  std::vector<uint8_t> wave;
  wave.reserve(44 + dataBytes);
  const auto tag = [&wave](const char* name) { wave.insert(wave.end(), name, name + 4); };
  const auto number = [&wave](unsigned value, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i) wave.push_back(static_cast<uint8_t>(value >> (8 * i)));
  };
  tag("RIFF"); number(36 + dataBytes, 4);
  tag("WAVE"); tag("fmt "); number(16, 4);
  number(1, 2); number(1, 2); number(sampleRate, 4); number(sampleRate * 2, 4);
  number(2, 2); number(16, 2); tag("data"); number(dataBytes, 4);
  for (const Note note : notes) {
    const auto count = note.milliseconds * sampleRate / 1000;
    for (unsigned i = 0; i < count; ++i) {
      const double seconds = static_cast<double>(i) / sampleRate;
      const double phase = 2 * std::numbers::pi * note.hz * seconds;
      const double attack = std::min(1.0, i / 144.0);
      const double release = 1.0 - static_cast<double>(i + 1) / count;
      const double tone = std::sin(phase) + .16 * std::sin(2 * phase);
      const auto pcm = static_cast<int16_t>(4400 * attack * release * release * tone);
      number(static_cast<uint16_t>(pcm), 2);
    }
  }
  return wave;
}

} // namespace paw
