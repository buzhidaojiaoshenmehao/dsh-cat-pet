#pragma once

#include <windows.h>
#include <gdiplus.h>

#include "animation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace paw {
namespace gd = Gdiplus;

inline const gd::Color ink(255, 48, 42, 37);
inline const gd::Color quiet(255, 111, 101, 91);
inline const gd::Color jade(255, 35, 103, 85);
inline const gd::Color peach(255, 196, 103, 102);
inline const gd::Color paper(255, 255, 251, 246);

inline void rounded(gd::Graphics& g, gd::RectF r, float radius, gd::Color color) {
  const float d = radius * 2;
  gd::GraphicsPath path;
  path.AddArc(r.X, r.Y, d, d, 180, 90);
  path.AddArc(r.GetRight() - d, r.Y, d, d, 270, 90);
  path.AddArc(r.GetRight() - d, r.GetBottom() - d, d, d, 0, 90);
  path.AddArc(r.X, r.GetBottom() - d, d, d, 90, 90);
  path.CloseFigure();
  gd::SolidBrush brush(color);
  g.FillPath(&brush, &path);
}

inline void label(gd::Graphics& g, const std::wstring& text, gd::RectF r, float size, gd::Color color = ink,
                  bool bold = false, gd::StringAlignment align = gd::StringAlignmentNear) {
  if (text.empty()) return;
  gd::Font font(L"Microsoft YaHei UI", size, bold ? gd::FontStyleBold : gd::FontStyleRegular, gd::UnitPixel);
  gd::StringFormat format;
  format.SetAlignment(align);
  format.SetLineAlignment(gd::StringAlignmentCenter);
  format.SetFormatFlags(gd::StringFormatFlagsNoWrap);
  format.SetTrimming(gd::StringTrimmingEllipsisCharacter);
  gd::SolidBrush brush(color);
  g.DrawString(text.c_str(), static_cast<INT>(text.size()), &font, r, &format, &brush);
}

inline void oval(gd::Graphics& g, gd::RectF rect, gd::Color color) {
  gd::SolidBrush brush(color);
  g.FillEllipse(&brush, rect);
}

inline void heart(gd::Graphics& g, float x, float y, float size, gd::Color color) {
  gd::GraphicsPath path;
  path.AddBezier(x, y + size * .24f, x - size * .55f, y - size * .37f,
                 x - size, y + size * .37f, x, y + size);
  path.AddBezier(x, y + size, x + size, y + size * .37f,
                 x + size * .55f, y - size * .37f, x, y + size * .24f);
  path.CloseFigure();
  gd::SolidBrush brush(color);
  g.FillPath(&brush, &path);
}

inline void configure(gd::Graphics& g, float scale) {
  g.SetSmoothingMode(gd::SmoothingModeAntiAlias);
  g.SetInterpolationMode(gd::InterpolationModeHighQualityBicubic);
  g.SetPixelOffsetMode(gd::PixelOffsetModeHalf);
  g.SetTextRenderingHint(gd::TextRenderingHintAntiAliasGridFit);
  g.ScaleTransform(scale, scale);
}

class LayerSurface {
public:
  LayerSurface() : dc_(CreateCompatibleDC(nullptr)) {}
  ~LayerSurface() {
    if (original_) SelectObject(dc_, original_);
    if (dib_) DeleteObject(dib_);
    if (dc_) DeleteDC(dc_);
  }
  LayerSurface(const LayerSurface&) = delete;
  LayerSurface& operator=(const LayerSurface&) = delete;

  bool hit(int screenX, int screenY) const {
    const int x = screenX - origin_.x, y = screenY - origin_.y;
    return pixels_ && x >= 0 && y >= 0 && x < width_ && y < height_
      && (pixels_[static_cast<size_t>(y) * width_ + x] >> 24) >= 28;
  }

  template <class Paint>
  void present(HWND window, int x, int y, int width, int height, float scale, bool input, Paint paint) {
    if (!resize(width, height)) return;
    {
      gd::Bitmap buffer(width_, height_, width_ * 4, PixelFormat32bppPARGB, reinterpret_cast<BYTE*>(pixels_));
      gd::Graphics graphics(&buffer);
      graphics.Clear(gd::Color(0, 0, 0, 0));
      configure(graphics, scale);
      paint(graphics);
      graphics.Flush(gd::FlushIntentionSync);
    }
    // Fully transparent pixels of layered windows pass input through to the desktop.
    if (input) {
      const size_t count = static_cast<size_t>(width_) * height_;
      for (size_t i = 0; i < count; ++i) if ((pixels_[i] >> 24) < 28) pixels_[i] = 0;
    }
    POINT source{};
    POINT destination{x, y};
    SIZE size{width_, height_};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    if (UpdateLayeredWindow(window, nullptr, &destination, &size, dc_, &source, 0, &blend, ULW_ALPHA)) {
      origin_ = destination;
    }
  }

private:
  bool resize(int w, int h) {
    if (!dc_ || w <= 0 || h <= 0) return false;
    if (dib_ && width_ == w && height_ == h) return true;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = w;
    info.bmiHeader.biHeight = -h;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* data = nullptr;
    HBITMAP next = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &data, nullptr, 0);
    if (!next || !data) { if (next) DeleteObject(next); return false; }
    HGDIOBJ previous = SelectObject(dc_, next);
    if (!original_) original_ = previous;
    if (dib_) DeleteObject(dib_);
    dib_ = next;
    pixels_ = static_cast<uint32_t*>(data);
    width_ = w;
    height_ = h;
    return true;
  }

  HDC dc_{};
  HBITMAP dib_{};
  HGDIOBJ original_{};
  uint32_t* pixels_{};
  int width_{}, height_{};
  POINT origin_{};
};

inline void drawSprite(gd::Graphics& g, gd::Bitmap& image, gd::RectF box, bool mirror = false) {
  const float width = static_cast<float>(image.GetWidth()), height = static_cast<float>(image.GetHeight());
  const float scale = std::min(box.Width / width, box.Height / height);
  const gd::RectF destination(box.X + (box.Width - width * scale) / 2,
    box.GetBottom() - height * scale, width * scale, height * scale);
  if (!mirror) {
    g.DrawImage(&image, destination);
    return;
  }
  const gd::GraphicsState saved = g.Save();
  g.TranslateTransform(destination.X * 2 + destination.Width, 0);
  g.ScaleTransform(-1, 1);
  g.DrawImage(&image, destination);
  g.Restore(saved);
}

inline void drawPose(gd::Graphics& g, gd::Bitmap& image, const Pose& pose, double center, double feet) {
  const auto bounds = spriteBounds(image.GetWidth(), image.GetHeight(), pose, center, feet);
  const gd::RectF frame(float(bounds.left), float(bounds.top), float(bounds.width()), float(bounds.height()));
  const auto saved = g.Save();
  if (pose.mirrored) {
    g.TranslateTransform(frame.X * 2 + frame.Width, 0);
    g.ScaleTransform(-1, 1);
  }
  g.DrawImage(&image, frame);

  if (pose.blink > 0) {
    for (const auto& eye : eyesFor(pose.sprite)) {
      const float x = frame.X + float(eye.x) * frame.Width, y = frame.Y + float(eye.y) * frame.Height;
      const float rx = float(eye.radiusX) * frame.Width, ry = float(eye.radiusY) * frame.Height;
      const gd::RectF lid(x - rx, y - ry, 2 * rx, 2 * ry);
      const auto eyeState = g.Save();
      gd::GraphicsPath outline;
      outline.AddEllipse(lid);
      g.SetClip(&outline, gd::CombineModeIntersect);
      g.SetClip(gd::RectF(lid.X, lid.Y, lid.Width, lid.Height * float(pose.blink)), gd::CombineModeIntersect);
      g.DrawImage(&image, gd::RectF(frame.X, frame.Y + 2.2f * ry, frame.Width, frame.Height));
      g.Restore(eyeState);
      if (pose.blink > .72) {
        const auto alpha = static_cast<BYTE>(178 * smoothStep((pose.blink - .72) / .28));
        gd::Pen seam(gd::Color(alpha, 33, 33, 33), .8f);
        g.DrawBezier(&seam, x - .8f * rx, y, x - .3f * rx, y + .32f * ry,
          x + .3f * rx, y + .32f * ry, x + .8f * rx, y);
      }
    }
  }
  g.Restore(saved);
}

} // namespace paw
