#define NOMINMAX
#include "geometry.hpp"
#include "graphics.hpp"
#include "protocol.hpp"
#include "sounds.hpp"
#include "tariff.hpp"
#include "pricing-calendar.hpp"

#include <commctrl.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <array>
#include <chrono>
#include <cwchar>
#include <deque>
#include <memory>
#include <random>

namespace {
using namespace paw;

constexpr UINT trayMessage = WM_APP + 1;
constexpr int refreshButton = 100;
constexpr int closeButton = 101;
constexpr int soundButton = 102, sizeSlider = 103;
constexpr UINT_PTR clockTimer = 2;
constexpr int minSize = 50, maxSize = 180;
constexpr float panelWidth = 380, panelHeight = 520;
enum MenuId { showBalance = 201, petCat, toggleRoam, toggleChase, toggleSound, bringHere, toggleVisible, quitPet };
enum SpriteId { idle, curious, pawUp, sleep, belly, crouch, walk1, walk2, walk3, walk4 };
constexpr std::array spriteNames{L"idle", L"curious", L"paw", L"sleep", L"belly", L"crouch",
  L"walk-1", L"walk-2", L"walk-3", L"walk-4"};

Point pointer() {
  POINT p{};
  GetCursorPos(&p);
  return {static_cast<double>(p.x), static_cast<double>(p.y)};
}

int roundedInt(double value) { return static_cast<int>(std::lround(value)); }

Rect fromWinRect(RECT rect) {
  return {static_cast<double>(rect.left), static_cast<double>(rect.top),
    static_cast<double>(rect.right), static_cast<double>(rect.bottom)};
}

BOOL CALLBACK collectScreen(HMONITOR monitor, HDC, LPRECT, LPARAM target) {
  MONITORINFO info{sizeof(info)};
  if (GetMonitorInfoW(monitor, &info)) {
    reinterpret_cast<std::vector<Screen>*>(target)->push_back({fromWinRect(info.rcMonitor), fromWinRect(info.rcWork)});
  }
  return TRUE;
}

std::wstring assetDirectory() {
  std::wstring path(32768, L'\0');
  const DWORD size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
  if (!size || size >= path.size()) return L"";
  path.resize(size);
  for (int i = 0; i < 2; ++i) {
    const auto separator = path.find_last_of(L"\\/");
    if (separator == std::wstring::npos) return L"";
    path.resize(separator);
  }
  return path + L"\\assets\\";
}

std::wstring settingsPath() {
  std::wstring path(32768, L'\0');
  const DWORD size = GetEnvironmentVariableW(L"LOCALAPPDATA", path.data(), static_cast<DWORD>(path.size()));
  if (!size || size >= path.size()) return {};
  path.resize(size);
  path += L"\\dsh-cat-pet";
  if (!CreateDirectoryW(path.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return {};
  return path + L"\\settings.ini";
}

std::wstring countdownText(int64_t seconds) {
  const auto days = seconds / 86400;
  const auto hours = seconds / 3600 % 24, minutes = seconds / 60 % 60;
  wchar_t clock[16]{};
  std::swprintf(clock, std::size(clock), L"%02d:%02d:%02d",
    static_cast<int>(hours), static_cast<int>(minutes), static_cast<int>(seconds % 60));
  return days ? std::to_wstring(days) + L" 天 " + clock : clock;
}

std::wstring beijingTime(int64_t milliseconds) {
  constexpr int64_t unixEpochInFileTime = 116444736000000000LL;
  const auto ticks = static_cast<uint64_t>(unixEpochInFileTime + (milliseconds + 8 * 3600000LL) * 10000);
  const FILETIME fileTime{static_cast<DWORD>(ticks), static_cast<DWORD>(ticks >> 32)};
  SYSTEMTIME date{};
  if (!FileTimeToSystemTime(&fileTime, &date)) return {};
  wchar_t text[24]{};
  std::swprintf(text, std::size(text), L"%02u/%02u %02u:%02u", unsigned(date.wMonth), unsigned(date.wDay),
    unsigned(date.wHour), unsigned(date.wMinute));
  return text;
}

class Pet {
public:
  Pet(HINSTANCE instance, bool demo) : instance_(instance), demo_(demo) {}

  ~Pet() {
    saveSize();
    PlaySoundW(nullptr, nullptr, 0);
    if (trayReady_) Shell_NotifyIconW(NIM_DELETE, &tray_);
    if (panel_) DestroyWindow(panel_);
    if (effects_) DestroyWindow(effects_);
    if (actor_) DestroyWindow(actor_);
    if (trayIcon_) DestroyIcon(trayIcon_);
    if (cardBrush_) DeleteObject(cardBrush_);
  }

  int run() {
    settings_ = settingsPath();
    if (!settings_.empty()) {
      const auto size = GetPrivateProfileIntW(L"pet", L"sizePercent", 100, settings_.c_str());
      if (size >= minSize && size <= maxSize) sizePercent_ = static_cast<int>(size);
      soundEnabled_ = GetPrivateProfileIntW(L"pet", L"sound", 0, settings_.c_str()) == 1;
    }
    if (!loadSprites() || !createWindows()) {
      MessageBoxW(nullptr, L"桌宠启动失败。请保留解压后的 bin 文件夹和素材。", L"小爪同伴", MB_OK | MB_ICONERROR);
      return 1;
    }
    input_ = GetStdHandle(STD_INPUT_HANDLE);
    output_ = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!demo_ && (GetFileType(input_) != FILE_TYPE_PIPE || GetFileType(output_) != FILE_TYPE_PIPE)) return 2;
    collectScreens();
    const Rect work = screens_[nearestScreen(screens_, pointer())].work;
    pos_ = safeArea(work, scale()).clamp({work.right - 148 * scale(), work.bottom - 32 * scale()});
    lastPointer_ = pointer();
    start_ = lastTick_ = startIdle_ = GetTickCount64();
    nextRoam_ = start_ + 2100;
    explorerRestart_ = RegisterWindowMessageW(L"TaskbarCreated");
    createTray();
    paintActor(start_);
    showPet();
    if (!SetTimer(actor_, 1, 33, nullptr)) return 3;
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
      if (panelOpen_ && (message.hwnd == panel_ || IsChild(panel_, message.hwnd))) {
        if (message.message == WM_KEYDOWN && message.wParam == VK_ESCAPE) { hidePanel(); continue; }
        if (IsDialogMessageW(panel_, &message)) continue;
      }
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
    KillTimer(actor_, 1);
    return 0;
  }

private:
  static LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    if (message == WM_NCCREATE) {
      const auto* created = reinterpret_cast<CREATESTRUCTW*>(lp);
      SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created->lpCreateParams));
    }
    const auto app = reinterpret_cast<Pet*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (!app) return DefWindowProcW(window, message, wp, lp);
    return app->dispatch(window, message, wp, lp);
  }

  bool loadSprites() {
    const std::wstring folder = assetDirectory();
    if (folder.empty()) return false;
    for (size_t i = 0; i < sprites_.size(); ++i) {
      sprites_[i] = std::make_unique<gd::Bitmap>((folder + spriteNames[i] + L".png").c_str());
      if (sprites_[i]->GetLastStatus() != gd::Ok || sprites_[i]->GetWidth() == 0) return false;
    }
    return true;
  }

  bool createWindows() {
    const INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_BAR_CLASSES};
    if (!InitCommonControlsEx(&controls)) return false;
    cardBrush_ = CreateSolidBrush(RGB(255, 251, 246));
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = windowProcedure;
    windowClass.hInstance = instance_;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = L"DshLittlePawActor";
    if (!RegisterClassExW(&windowClass)) return false;
    windowClass.style = CS_DROPSHADOW;
    windowClass.lpszClassName = L"DshLittlePawPanel";
    if (!RegisterClassExW(&windowClass)) return false;
    constexpr DWORD layered = WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOPMOST;
    effects_ = CreateWindowExW(layered | WS_EX_TRANSPARENT, L"DshLittlePawActor", L"小爪同伴 · 动画", WS_POPUP,
      0, 0, 1, 1, nullptr, nullptr, instance_, this);
    actor_ = CreateWindowExW(layered, L"DshLittlePawActor", L"小爪同伴", WS_POPUP,
      0, 0, 1, 1, nullptr, nullptr, instance_, this);
    if (!actor_ || !effects_) return false;
    panel_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_CONTROLPARENT,
      L"DshLittlePawPanel", L"小爪同伴 · 余额与设置", WS_POPUP | WS_CLIPCHILDREN,
      0, 0, 1, 1, actor_, nullptr, instance_, this);
    if (!panel_) return false;
    refresh_ = makeButton(refreshButton, L"刷新余额");
    close_ = makeButton(closeButton, L"关闭余额面板");
    sound_ = makeButton(soundButton, soundEnabled_ ? L"音效开" : L"音效关");
    slider_ = CreateWindowExW(0, TRACKBAR_CLASSW, L"桌宠大小", WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_NOTICKS,
      0, 0, 1, 1, panel_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(sizeSlider)), instance_, nullptr);
    if (!refresh_ || !close_ || !sound_ || !slider_) return false;
    SendMessageW(slider_, TBM_SETRANGEMIN, FALSE, minSize);
    SendMessageW(slider_, TBM_SETRANGEMAX, FALSE, maxSize);
    SendMessageW(slider_, TBM_SETLINESIZE, 0, 1);
    SendMessageW(slider_, TBM_SETPAGESIZE, 0, 10);
    SendMessageW(slider_, TBM_SETPOS, TRUE, sizePercent_);
    return true;
  }

  HWND makeButton(int id, const wchar_t* title) const {
    return CreateWindowExW(0, L"BUTTON", title, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
      0, 0, 1, 1, panel_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr);
  }

  void collectScreens() {
    screens_.clear();
    EnumDisplayMonitors(nullptr, nullptr, collectScreen, reinterpret_cast<LPARAM>(&screens_));
    if (screens_.empty()) {
      RECT work{};
      SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
      screens_.push_back({fromWinRect(work), fromWinRect(work)});
    }
  }

  float displayScale() const { return std::max(96u, GetDpiForWindow(actor_)) / 96.0f; }
  float scale() const { return displayScale() * sizePercent_ / 100.0f; }

  void saveSize() {
    if (!saveSizeAt_) return;
    saveSizeAt_ = 0;
    if (settings_.empty()) return;
    WritePrivateProfileStringW(L"pet", L"sizePercent", std::to_wstring(sizePercent_).c_str(), settings_.c_str());
  }

  void resizePet() {
    const auto size = static_cast<int>(SendMessageW(slider_, TBM_GETPOS, 0, 0));
    if (size == sizePercent_) return;
    sizePercent_ = size;
    path_.clear();
    pos_ = safeArea(screens_[nearestScreen(screens_, pos_)].work, scale()).clamp(pos_);
    saveSizeAt_ = GetTickCount64() + 400;
    InvalidateRect(panel_, nullptr, FALSE);
  }

  void playChime(bool completed) const {
    if (!soundEnabled_) return;
    static const auto pet = makeChime({{659.25, 68}, {880, 115}});
    static const auto finish = makeChime({{523.25, 88}, {659.25, 88}, {783.99, 195}});
    const auto& wave = completed ? finish : pet;
    PlaySoundW(reinterpret_cast<LPCWSTR>(wave.data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
  }

  void toggleChime() {
    soundEnabled_ = !soundEnabled_;
    if (!settings_.empty()) WritePrivateProfileStringW(L"pet", L"sound", soundEnabled_ ? L"1" : L"0", settings_.c_str());
    SetWindowTextW(sound_, soundEnabled_ ? L"音效开" : L"音效关");
    InvalidateRect(sound_, nullptr, FALSE);
    if (soundEnabled_) playChime(false);
    else PlaySoundW(nullptr, nullptr, 0);
  }

  void createTray() {
    if (!trayIcon_) {
      gd::Bitmap icon(32, 32, PixelFormat32bppARGB);
      gd::Graphics g(&icon);
      configure(g, 1);
      rounded(g, {0, 0, 32, 32}, 8, gd::Color(255, 255, 240, 224));
      drawSprite(g, *sprites_[idle], {0, 1, 32, 31});
      g.Flush(gd::FlushIntentionSync);
      icon.GetHICON(&trayIcon_);
    }
    tray_ = {};
    tray_.cbSize = sizeof(tray_);
    tray_.hWnd = actor_;
    tray_.uID = 1;
    tray_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    tray_.uCallbackMessage = trayMessage;
    tray_.hIcon = trayIcon_ ? trayIcon_ : LoadIconW(nullptr, IDI_APPLICATION);
    lstrcpynW(tray_.szTip, demo_ ? L"小爪同伴 · 演示" : L"小爪同伴 · DeepSeek Harness", 128);
    trayReady_ = Shell_NotifyIconW(NIM_ADD, &tray_) != FALSE;
  }

  void showPet() {
    hidden_ = false;
    SetWindowPos(effects_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SetWindowPos(actor_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
  }

  void hidePet() {
    hidePanel();
    hidden_ = true;
    toast_.reset();
    toasts_.clear();
    ShowWindow(actor_, SW_HIDE);
    ShowWindow(effects_, SW_HIDE);
  }

  void placePanel() {
    const Rect work = screens_[nearestScreen(screens_, pos_)].work;
    panelScale_ = std::min({static_cast<double>(displayScale()), (work.width() - 12) / panelWidth,
      (work.height() - 12) / panelHeight});
    panelScale_ = std::max(.45, panelScale_);
    const double w = panelWidth * panelScale_, h = panelHeight * panelScale_;
    double x = pos_.x + 102 * scale();
    if (x + w > work.right) x = pos_.x - 102 * scale() - w;
    x = std::clamp(x, work.left + 5, std::max(work.left + 5, work.right - w - 5));
    const double y = std::clamp(pos_.y - h + 24, work.top + 5, std::max(work.top + 5, work.bottom - h - 5));
    SetWindowPos(panel_, HWND_TOPMOST, roundedInt(x), roundedInt(y), roundedInt(w), roundedInt(h), SWP_NOACTIVATE);
    const int radius = roundedInt(22 * panelScale_);
    const HRGN shape = CreateRoundRectRgn(0, 0, roundedInt(w) + 1, roundedInt(h) + 1, radius, radius);
    if (!SetWindowRgn(panel_, shape, TRUE)) DeleteObject(shape);
    placeButton(refresh_, 287, 75, 62, 27);
    placeButton(close_, 329, 15, 33, 30);
    placeButton(slider_, 27, 479, 191, 23);
    placeButton(sound_, 232, 465, 116, 30);
  }

  void placeButton(HWND button, int x, int y, int w, int h) const {
    MoveWindow(button, roundedInt(x * panelScale_), roundedInt(y * panelScale_),
      roundedInt(w * panelScale_), roundedInt(h * panelScale_), TRUE);
  }

  void openPanel() {
    if (hidden_) showPet();
    path_.clear();
    placePanel();
    panelOpen_ = true;
    ShowWindow(panel_, SW_SHOW);
    SetForegroundWindow(panel_);
    SetFocus(refresh_);
    SetTimer(panel_, clockTimer, 1000, nullptr);
    requestBalance();
  }

  void hidePanel() {
    panelOpen_ = false;
    saveSize();
    if (panel_) {
      KillTimer(panel_, clockTimer);
      ShowWindow(panel_, SW_HIDE);
    }
  }

  void requestBalance() {
    if (demo_) {
      balance_ = {"ready", "api", "1", {{"CNY", "28.60", "3.60", "25.00"}}};
      GetLocalTime(&balanceTime_);
    } else if (balance_.status != "loading") {
      balance_.status = "loading";
      writePipe("balance\n");
    }
    InvalidateRect(panel_, nullptr, FALSE);
  }

  void writePipe(std::string_view command) const {
    DWORD written = 0;
    WriteFile(output_, command.data(), static_cast<DWORD>(command.size()), &written, nullptr);
  }

  void quit(bool byUser) {
    if (byUser && !demo_) writePipe("exit\n");
    PostQuitMessage(0);
  }

  bool readPipe() {
    for (int chunk = 0; chunk < 8; ++chunk) {
      DWORD available = 0;
      if (!PeekNamedPipe(input_, nullptr, 0, nullptr, &available, nullptr)) return false;
      if (!available) return true;
      char data[4096];
      DWORD read = 0;
      if (!ReadFile(input_, data, std::min<DWORD>(available, sizeof(data)), &read, nullptr) || !read) return false;
      pipeBuffer_.append(data, read);
      size_t end;
      while ((end = pipeBuffer_.find('\n')) != std::string::npos) {
        std::string line = pipeBuffer_.substr(0, end);
        pipeBuffer_.erase(0, end + 1);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() <= 4096 && !receive(line)) return false;
      }
      if (pipeBuffer_.size() > 4096) return false;
    }
    return true;
  }

  bool receive(const std::string& line) {
    const auto f = fields(line);
    if (f[0] == "quit") return false;
    if (f[0] == "hello") { linked_ = f.size() == 2 && f[1] == "1"; return linked_; }
    if (!linked_) return true;
    if (f[0] == "activity" && f.size() == 3) {
      setActivity(f[1], f[2]);
    } else if (f[0] == "task" || f[0] == "recent") {
      if (const auto task = parseTask(f)) acceptTask(*task, f[0] == "task");
    } else if (f[0] == "balance") {
      auto next = parseBalance(f);
      if (!next) return true;
      if (next->status == "ready") GetLocalTime(&balanceTime_);
      // Keep the last currency for the task receipt while refreshing the balance.
      if (next->status == "loading") balance_.status = next->status;
      else balance_ = std::move(*next);
      InvalidateRect(panel_, nullptr, FALSE);
    }
    return true;
  }

  void setActivity(const std::string& value, const std::string& count) {
    if (activity_ == value && count_ == count) return;
    activity_ = value;
    count_ = count;
    if (activity_ == "idle") startIdle_ = GetTickCount64();
    if (activity_ == "working") nextRoam_ = std::min(nextRoam_, GetTickCount64() + 900);
    InvalidateRect(panel_, nullptr, FALSE);
  }

  void acceptTask(const Task& task, bool announce) {
    recent_ = task;
    if (announce && !hidden_) {
      if (toasts_.size() >= 16) toasts_.pop_front();
      toasts_.push_back(task);
    }
    InvalidateRect(panel_, nullptr, FALSE);
  }

  void advanceDemo(ULONGLONG now) {
    const auto elapsed = now - start_;
    const auto phase = elapsed % 26000;
    if (phase >= 3100 && phase < 6700) setActivity("thinking", "1");
    else if (phase >= 6700 && phase < 11600) setActivity("working", "1");
    else setActivity("idle", "0");
    const auto cycle = elapsed / 26000;
    if (phase >= 11600 && demoCycle_ != cycle) {
      demoCycle_ = cycle;
      acceptTask({"completed", "main", "4680", "4200", "480", "3200", "0.002984", "0.000448", "full", "api"}, true);
    }
  }

  void play(ULONGLONG now, bool bellyRub = false) {
    playUntil_ = now + (bellyRub ? 1500 : 950);
    sparkAt_ = now;
    rub_ = bellyRub;
    path_.clear();
    nextRoam_ = std::max(nextRoam_, playUntil_ + 2200);
    if (now >= nextPlaySound_) {
      playChime(false);
      nextPlaySound_ = now + 1400;
    }
  }

  void chooseRoute(float dpi) {
    const size_t from = nearestScreen(screens_, pos_);
    size_t to = from;
    if (++roamCount_ % 3 == 0 && screens_.size() > 1) to = (from + 1) % screens_.size();
    Rect area = safeArea(screens_[to].work, dpi);
    if (activity_ == "working" && from == to) {
      area = {std::max(area.left, pos_.x - 230 * dpi), std::max(area.top, pos_.y - 120 * dpi),
        std::min(area.right, pos_.x + 230 * dpi), std::min(area.bottom, pos_.y + 120 * dpi)};
    }
    std::uniform_real_distribution<double> fraction(.12, .88);
    const Point goal{area.left + fraction(random_) * area.width(), area.top + fraction(random_) * area.height()};
    auto points = route(screens_, from, to, goal);
    if (points.empty()) points.push_back(safeArea(screens_[from].work, dpi).center());
    path_.assign(points.begin(), points.end());
  }

  void stepMotion(Point goal, double maxDistance) {
    const Point vector = goal - pos_;
    const double remaining = length(vector);
    if (remaining < .1) return;
    const double distance = std::min(remaining, maxDistance);
    if (std::abs(vector.x) > 3) facingRight_ = vector.x > 0;
    pos_ = pos_ + vector * (distance / remaining);
    walkPixels_ += distance / scale();
    walking_ = true;
  }

  void movement(ULONGLONG now, double dt, Point mouse, float dpi) {
    walking_ = false;
    if (length(mouse - lastPointer_) >= 2) lastMove_ = now;
    lastPointer_ = mouse;
    if (menuOpen_ || panelOpen_) return;
    const bool onBody = actorSurface_.hit(roundedInt(mouse.x), roundedInt(mouse.y));
    if (pressed_) {
      if (length(mouse - pressPoint_) >= 6 * dpi) dragged_ = true;
      if (dragged_) pos_ = safeArea(screens_[nearestScreen(screens_, mouse)].work, dpi).clamp(mouse + pressOffset_);
      if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) releasePointer();
      return;
    }
    if (onBody) {
      if (!hoverAt_) { hoverAt_ = now; if (now >= playUntil_) play(now); }
      else if (!rub_ && now - hoverAt_ >= 1000 && now - hoverAt_ < 1800) play(now, true);
      return;
    }
    hoverAt_ = 0;
    if (toast_ || now < playUntil_) return;

    const Point center = pos_ - Point{0, 60 * dpi};
    const double cursorDistance = length(mouse - center);
    if (chase_ && now - lastMove_ < 650 && cursorDistance < 170 * dpi && now >= chaseAfter_) {
      const size_t from = nearestScreen(screens_, pos_);
      const Rect area = safeArea(screens_[from].work, dpi);
      const double side = mouse.x > center.x ? -1 : 1;
      const Point goal = area.clamp({mouse.x + side * 37 * dpi, mouse.y + 64 * dpi});
      if (cursorDistance < 63 * dpi || length(goal - pos_) < 9 * dpi) {
        facingRight_ = mouse.x > center.x;
        play(now);
        catchAt_ = now;
        catchPoint_ = mouse;
        chaseAfter_ = now + 2100;
      } else {
        path_.clear();
        stepMotion(goal, dt * 215 * dpi);
      }
      return;
    }

    if (!roam_) return;
    if (path_.empty() && now >= nextRoam_) chooseRoute(dpi);
    while (!path_.empty() && length(path_.front() - pos_) <= 3 * dpi) {
      pos_ = path_.front();
      path_.pop_front();
      if (path_.empty()) nextRoam_ = now + (activity_ == "working" ? 1900 : 4700);
    }
    if (!path_.empty()) stepMotion(path_.front(), dt * (activity_ == "working" ? 116 : 88) * dpi);
  }

  std::wstring activityLabel() const {
    if (activity_ == "working") return L"执行工具";
    if (activity_ == "thinking") return L"思考中";
    return linked_ || demo_ ? L"待命" : L"连接中";
  }

  void tick() {
    const auto now = GetTickCount64();
    const double dt = std::min(0.06, static_cast<double>(now - lastTick_) / 1000);
    lastTick_ = now;
    if (saveSizeAt_ && now >= saveSizeAt_) saveSize();
    if (demo_) advanceDemo(now);
    else if (!readPipe()) { quit(false); return; }
    if (hidden_) return;
    if (toast_ && now >= toastUntil_) toast_.reset();
    if (!toast_ && !toasts_.empty()) {
      toast_ = toasts_.front();
      toasts_.pop_front();
      toastUntil_ = now + 4600;
      if (toast_->outcome == "completed") playChime(true);
    }
    movement(now, dt, pointer(), scale());
    paintActor(now);
  }

  void paintActor(ULONGLONG now) {
    const float dpi = scale();
    const auto seconds = static_cast<double>(now - start_) / 1000;
    SpriteId sprite = idle;
    bool mirror = false;
    float width = 158, height = 142;
    float lift = static_cast<float>(std::sin(seconds * 2.5) * 1.25);
    if (now < playUntil_) {
      sprite = rub_ ? belly : pawUp;
      mirror = !rub_ && facingRight_;
      width = rub_ ? 188 : 158;
      height = rub_ ? 114 : 144;
      lift += static_cast<float>(std::sin(seconds * 12) * 2.4);
    } else if (toast_) {
      sprite = toast_->outcome == "completed" ? belly : curious;
      width = 188;
      height = 126;
      const double age = static_cast<double>(4600 - (toastUntil_ - now)) / 1000;
      if (age < .7) lift -= static_cast<float>(14 * std::abs(std::sin(age * 4.5)));
    } else if (walking_) {
      sprite = static_cast<SpriteId>(walk1 + static_cast<int>(walkPixels_ / 11) % 4);
      mirror = facingRight_;
      width = 207;
      height = 119;
      lift -= static_cast<float>(2 * std::abs(std::sin(walkPixels_ / 11 * 1.57)));
    } else if (activity_ == "working") {
      sprite = crouch;
      width = 179;
      height = 104;
    } else if (activity_ == "thinking") {
      sprite = curious;
    } else if ((startIdle_ && now - startIdle_ > 17000) || (demo_ && (now - start_) % 26000 > 18800)) {
      sprite = sleep;
      width = 173;
      height = 97;
    }
    constexpr int actorWidth = 232, actorHeight = 178;
    const int x = roundedInt(pos_.x - actorWidth * dpi / 2);
    const int y = roundedInt(pos_.y - (actorHeight - 7) * dpi);
    actorSurface_.present(actor_, x, y, roundedInt(actorWidth * dpi), roundedInt(actorHeight * dpi), dpi, true,
      [&](gd::Graphics& g) {
        drawSprite(g, *sprites_[sprite], {(actorWidth - width) / 2, actorHeight - 7 - height + lift, width, height}, mirror);
      });
    paintEffects(now, dpi, sprite, seconds);
  }

  void paintEffects(ULONGLONG now, float dpi, SpriteId sprite, double seconds) {
    constexpr int width = 292, height = 246;
    const Rect work = screens_[nearestScreen(screens_, pos_)].work;
    const int x = roundedInt(std::clamp(pos_.x - width * dpi / 2, work.left,
      std::max(work.left, work.right - width * dpi)));
    const int y = roundedInt(pos_.y - (height - 18) * dpi);
    const float center = static_cast<float>((pos_.x - x) / dpi);
    const float badgeY = std::max(6.0f, static_cast<float>((work.top - y) / dpi + 5));
    effectsSurface_.present(effects_, x, y, roundedInt(width * dpi), roundedInt(height * dpi), dpi, false,
      [&](gd::Graphics& g) {
        for (int layer = 0; layer < 4; ++layer) {
          oval(g, {center - 61.0f + layer * 6, 222.0f + layer, 122.0f - layer * 12, 14.0f - layer * 2},
            gd::Color(18, 46, 32, 29));
        }
        if (toast_) {
          const float top = std::min(103.0f, badgeY + 7);
          rounded(g, {13, top + 3, 266, 65}, 15, gd::Color(65, 51, 34, 24));
          rounded(g, {13, top, 266, 65}, 15, paper);
          heart(g, 31, top + 14, 10, toast_->outcome == "completed" ? peach : quiet);
          label(g, (demo_ ? L"演示 · " : L"") + taskTitle(*toast_), {46, top + 8, 212, 21}, 14, ink, true);
          label(g, tokenText(*toast_) + L"  ·  " + taskPrice(*toast_, balance_),
            {25, top + 33, 245, 23}, 12, jade, true);
        } else if (activity_ != "idle") {
          const std::wstring count = count_ != "1" && count_ != "0" ? L" · " + ascii(count_) : L"";
          const float top = std::min(115.0f, badgeY + 44);
          rounded(g, {77, top, 138, 28}, 14, gd::Color(246, 255, 251, 246));
          const float beat = static_cast<float>(std::sin(seconds * 6) * 1.0);
          oval(g, {89 - beat / 2, top + 11 - beat / 2, 6 + beat, 6 + beat}, jade);
          label(g, activityLabel() + count, {101, top, 105, 28}, 12, jade, true);
        } else if (sprite == sleep) {
          label(g, L"z z z", {center + 31, 107 + static_cast<float>(std::sin(seconds * 2) * 4), 52, 24}, 15, jade, true);
        }
        if (sparkAt_ && now - sparkAt_ < 1250) {
          const float t = static_cast<float>(now - sparkAt_) / 1250;
          const BYTE alpha = static_cast<BYTE>(240 * (1 - t));
          for (int i = 0; i < 4; ++i) {
            const float direction = static_cast<float>(i) - 1.5f;
            const float sx = center + direction * (22 + 30 * t);
            const float sy = 135 - t * (64 + i * 9);
            heart(g, sx, sy, 10 - i * 1.1f, gd::Color(alpha, 222, 111, 117));
          }
        }
        if (catchAt_ && now - catchAt_ < 480) {
          const float t = static_cast<float>(now - catchAt_) / 480;
          const float r = 5 + t * 11;
          const float cx = static_cast<float>((catchPoint_.x - x) / dpi);
          const float cy = static_cast<float>((catchPoint_.y - y) / dpi);
          gd::Pen ring(gd::Color(static_cast<BYTE>(210 * (1 - t)), 223, 158, 102), 2.5f);
          g.DrawEllipse(&ring, cx - r, cy - r, r * 2, r * 2);
        }
      });
  }

  void paintTariff(gd::Graphics& g) const {
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
    const auto window = tariffWindowAt(tariffCalendar, now);
    rounded(g, {16, 213, 348, 78}, 15, paper);
    label(g, L"峰谷倒计时", {29, 220, 96, 25}, 15, ink, true);
    label(g, L"北京时间", {130, 221, 80, 24}, 10, quiet);
    if (window.current != Period::unknown) {
      const bool peak = window.current == Period::peak;
      const auto color = peak ? gd::Color(255, 153, 77, 39) : jade;
      rounded(g, {273, 222, 76, 21}, 10,
        peak ? gd::Color(255, 251, 230, 213) : gd::Color(255, 229, 239, 228));
      label(g, peak ? L"高峰时段" : L"空闲时段", {273, 222, 76, 21}, 10, color, true, gd::StringAlignmentCenter);
    }
    if (window.next) {
      label(g, countdownText(secondsUntil(now, window.next->at)), {29, 247, 175, 31}, 21, ink, true);
      const std::wstring next = window.next->period == Period::peak ? L"下次高峰  " : L"下次空闲  ";
      label(g, next + beijingTime(window.next->at), {192, 249, 156, 30}, 11, quiet, false, gd::StringAlignmentFar);
    } else {
      label(g, L"后续计费日历待更新", {29, 247, 319, 31}, 13, quiet);
    }
  }

  void paintPanel() const {
    PAINTSTRUCT paint{};
    HDC dc = BeginPaint(panel_, &paint);
    {
      gd::Graphics g(dc);
      configure(g, static_cast<float>(panelScale_));
      g.Clear(gd::Color(255, 250, 244, 237));
      rounded(g, {17, 14, 44, 43}, 12, gd::Color(255, 242, 225, 209));
      drawSprite(g, *sprites_[idle], {20, 16, 38, 41});
      label(g, L"小爪同伴", {71, 12, 243, 28}, 21, ink, true);
      label(g, (demo_ ? L"演示数据" : L"DeepSeek Harness") + std::wstring(L"  ·  ") + activityLabel(),
        {71, 39, 243, 19}, 11, quiet);

      rounded(g, {16, 66, 348, 136}, 15, paper);
      label(g, L"DeepSeek 余额", {29, 77, 146, 23}, 15, ink, true);
      if (balance_.status == "ready" && !balance_.wallets.empty()) {
        const bool dual = balance_.wallets.size() == 2;
        const float column = dual ? 163.0f : 315.0f;
        for (size_t i = 0; i < balance_.wallets.size(); ++i) {
          const Wallet& wallet = balance_.wallets[i];
          const float x = 29 + i * column;
          label(g, money(wallet.currency, wallet.total), {x, 102, column - 8, 38}, dual ? 23 : 28, jade, true);
          label(g, L"赠送 " + money(wallet.currency, wallet.bonus), {x, 141, column - 8, 18}, 11, quiet);
          label(g, L"充值 " + money(wallet.currency, wallet.cash), {x, 157, column - 8, 18}, 11, quiet);
        }
        wchar_t stamp[32]{};
        swprintf(stamp, 32, L"%02u:%02u:%02u", balanceTime_.wHour, balanceTime_.wMinute, balanceTime_.wSecond);
        const std::wstring source = balance_.mode == "account" ? L"平台账号" : L"API Key";
        label(g, source + L"  ·  " + stamp + (balance_.available == "0" ? L"  ·  余额不足" : L""),
          {29, 177, 320, 17}, 10, quiet);
      } else {
        label(g, balanceMessage(balance_.status), {29, 114, 322, 52}, 13, quiet);
      }

      paintTariff(g);
      rounded(g, {16, 302, 348, 137}, 15, paper);
      label(g, L"最近任务", {29, 313, 120, 23}, 15, ink, true);
      if (recent_) {
        const Task& task = *recent_;
        label(g, taskTitle(task), {176, 313, 172, 23}, 11, jade, false, gd::StringAlignmentFar);
        label(g, tokenText(task), {29, 339, 194, 35}, 19, ink, true);
        label(g, taskPrice(task, balance_), {196, 339, 152, 35}, 12, jade, true, gd::StringAlignmentFar);
        label(g, L"输入 " + grouped(task.input) + L"   ·   输出 " + grouped(task.output),
          {29, 375, 319, 20}, 11, quiet);
        label(g, L"缓存命中 " + grouped(task.cached), {29, 394, 319, 19}, 11, quiet);
        const std::wstring note = task.incomplete ? L"部分用量未返回" : task.coverage == "full"
          ? L"模型费用按官方峰谷价估算" : task.coverage == "partial" ? L"仅显示可计价的模型费用" : L"当前模型费用无可用价格";
        label(g, note, {29, 413, 319, 18}, 10, quiet);
      } else {
        label(g, demo_ ? L"演示任务结束后会在这里显示账单" : L"任务结束后会在这里显示用量和预估费用",
          {29, 351, 319, 36}, 13, quiet);
      }

      rounded(g, {16, 450, 348, 56}, 15, paper);
      label(g, L"桌宠大小", {29, 456, 97, 23}, 13, ink, true);
      label(g, std::to_wstring(sizePercent_) + L"%", {158, 456, 57, 23}, 12, jade, true, gd::StringAlignmentFar);
    }
    EndPaint(panel_, &paint);
  }

  void paintButton(const DRAWITEMSTRUCT& item) const {
    gd::Graphics g(item.hDC);
    configure(g, static_cast<float>(panelScale_));
    const float w = (item.rcItem.right - item.rcItem.left) / static_cast<float>(panelScale_);
    const float h = (item.rcItem.bottom - item.rcItem.top) / static_cast<float>(panelScale_);
    g.Clear(item.CtlID == closeButton ? gd::Color(255, 250, 244, 237) : paper);
    rounded(g, {0, 0, w, h}, 8, (item.itemState & ODS_SELECTED) ? gd::Color(255, 207, 226, 215) : gd::Color(255, 235, 239, 231));
    const wchar_t* title = item.CtlID == closeButton ? L"×" : item.CtlID == refreshButton ? L"刷新"
      : soundEnabled_ ? L"音效 · 开" : L"音效 · 关";
    label(g, title, {0, 0, w, h}, item.CtlID == closeButton ? 19.0f : 12.0f,
      jade, true, gd::StringAlignmentCenter);
    if (item.itemState & ODS_FOCUS) {
      gd::Pen pen(jade, 1);
      g.DrawRectangle(&pen, 3.0f, 3.0f, w - 6, h - 6);
    }
  }

  void menu() {
    if (menuOpen_) return;
    menuOpen_ = true;
    const Point mouse = pointer();
    HMENU actions = CreatePopupMenu();
    AppendMenuW(actions, MF_STRING, showBalance, L"余额与最近任务");
    AppendMenuW(actions, MF_STRING, petCat, L"摸摸猫");
    AppendMenuW(actions, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(actions, MF_STRING | (roam_ ? MF_CHECKED : MF_UNCHECKED), toggleRoam, L"桌面巡游");
    AppendMenuW(actions, MF_STRING | (chase_ ? MF_CHECKED : MF_UNCHECKED), toggleChase, L"追光标");
    AppendMenuW(actions, MF_STRING | (soundEnabled_ ? MF_CHECKED : MF_UNCHECKED), toggleSound, L"音效");
    AppendMenuW(actions, MF_STRING, bringHere, L"叫到当前屏幕");
    AppendMenuW(actions, MF_STRING | (trayReady_ ? MF_ENABLED : MF_GRAYED), toggleVisible, hidden_ ? L"显示桌宠" : L"隐藏桌宠");
    AppendMenuW(actions, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(actions, MF_STRING, quitPet, L"退出桌宠");
    SetForegroundWindow(actor_);
    const UINT command = TrackPopupMenu(actions, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
      roundedInt(mouse.x), roundedInt(mouse.y), 0, actor_, nullptr);
    DestroyMenu(actions);
    PostMessageW(actor_, WM_NULL, 0, 0);
    menuOpen_ = false;
    switch (command) {
      case showBalance: openPanel(); break;
      case petCat: showPet(); play(GetTickCount64(), true); break;
      case toggleRoam: roam_ = !roam_; path_.clear(); break;
      case toggleChase: chase_ = !chase_; break;
      case toggleSound: toggleChime(); break;
      case bringHere:
        path_.clear();
        pos_ = safeArea(screens_[nearestScreen(screens_, mouse)].work, scale()).clamp(mouse + Point{88, 95});
        paintActor(GetTickCount64());
        showPet();
        break;
      case toggleVisible: if (hidden_) showPet(); else hidePet(); break;
      case quitPet: quit(true); break;
    }
  }

  void releasePointer() {
    const bool click = pressed_ && !dragged_ && length(pointer() - pressPoint_) < 6 * scale();
    pressed_ = false;
    if (GetCapture() == actor_) ReleaseCapture();
    if (click) openPanel();
  }

  LRESULT dispatch(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    if (window == panel_) {
      switch (message) {
        case WM_PAINT: paintPanel(); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_DRAWITEM: paintButton(*reinterpret_cast<DRAWITEMSTRUCT*>(lp)); return TRUE;
        case WM_CTLCOLORSTATIC:
          if (reinterpret_cast<HWND>(lp) == slider_ && cardBrush_) {
            SetBkColor(reinterpret_cast<HDC>(wp), RGB(255, 251, 246));
            return reinterpret_cast<LRESULT>(cardBrush_);
          }
          break;
        case WM_TIMER:
          if (wp == clockTimer) {
            RECT area{0, roundedInt(212 * panelScale_), roundedInt(panelWidth * panelScale_), roundedInt(293 * panelScale_)};
            InvalidateRect(panel_, &area, FALSE);
          }
          return 0;
        case WM_HSCROLL:
          if (reinterpret_cast<HWND>(lp) == slider_) resizePet();
          return 0;
        case WM_COMMAND:
          if (LOWORD(wp) == refreshButton) requestBalance();
          if (LOWORD(wp) == soundButton) toggleChime();
          if (LOWORD(wp) == closeButton || LOWORD(wp) == IDCANCEL) hidePanel();
          return 0;
        case WM_CLOSE: hidePanel(); return 0;
        case WM_ACTIVATE: if (LOWORD(wp) == WA_INACTIVE && panelOpen_) hidePanel(); break;
      }
    }
    if (window == actor_) {
      if (explorerRestart_ && message == explorerRestart_) { createTray(); return 0; }
      switch (message) {
        case WM_TIMER: tick(); return 0;
        case WM_DISPLAYCHANGE:
        case WM_SETTINGCHANGE:
          collectScreens();
          path_.clear();
          pos_ = safeArea(screens_[nearestScreen(screens_, pos_)].work, scale()).clamp(pos_);
          if (panelOpen_) placePanel();
          return 0;
        case WM_NCHITTEST: {
          const Point mouse = pointer();
          return actorSurface_.hit(roundedInt(mouse.x), roundedInt(mouse.y)) ? HTCLIENT : HTTRANSPARENT;
        }
        case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
        case WM_SETCURSOR:
          if (LOWORD(lp) == HTCLIENT) { SetCursor(LoadCursorW(nullptr, IDC_HAND)); return TRUE; }
          break;
        case WM_LBUTTONDOWN:
          pressed_ = true;
          dragged_ = false;
          pressPoint_ = pointer();
          pressOffset_ = pos_ - pressPoint_;
          path_.clear();
          SetCapture(actor_);
          return 0;
        case WM_LBUTTONUP: releasePointer(); return 0;
        case WM_CAPTURECHANGED: pressed_ = false; return 0;
        case WM_RBUTTONUP: menu(); return 0;
        case trayMessage:
          if (lp == WM_LBUTTONUP) { showPet(); openPanel(); }
          if (lp == WM_RBUTTONUP) menu();
          return 0;
        case WM_CLOSE: quit(true); return 0;
        case WM_QUERYENDSESSION: return TRUE;
        case WM_ENDSESSION: if (wp) quit(false); return 0;
      }
    }
    return DefWindowProcW(window, message, wp, lp);
  }

  HINSTANCE instance_{};
  HWND actor_{}, effects_{}, panel_{}, refresh_{}, close_{}, slider_{}, sound_{};
  HICON trayIcon_{};
  HBRUSH cardBrush_{};
  NOTIFYICONDATAW tray_{};
  HANDLE input_{}, output_{};
  UINT explorerRestart_{};
  bool demo_{}, linked_{}, trayReady_{}, hidden_{}, panelOpen_{}, menuOpen_{};
  bool roam_{true}, chase_{true}, walking_{}, facingRight_{}, pressed_{}, dragged_{}, rub_{};
  bool soundEnabled_{};
  int sizePercent_{100};
  std::wstring settings_;
  double panelScale_{1}, walkPixels_{};
  unsigned roamCount_{};
  Point pos_{}, lastPointer_{}, pressPoint_{}, pressOffset_{}, catchPoint_{};
  ULONGLONG start_{}, lastTick_{}, nextRoam_{}, lastMove_{}, startIdle_{};
  ULONGLONG hoverAt_{}, playUntil_{}, sparkAt_{}, catchAt_{}, chaseAfter_{}, toastUntil_{}, demoCycle_{~0ULL};
  ULONGLONG saveSizeAt_{}, nextPlaySound_{};
  std::vector<Screen> screens_;
  std::deque<Point> path_;
  std::mt19937 random_{std::random_device{}()};
  std::array<std::unique_ptr<gd::Bitmap>, spriteNames.size()> sprites_;
  LayerSurface actorSurface_, effectsSurface_;
  std::string pipeBuffer_, activity_{"idle"}, count_{"0"};
  Balance balance_;
  SYSTEMTIME balanceTime_{};
  std::optional<Task> recent_, toast_;
  std::deque<Task> toasts_;
};
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR arguments, int) {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  Gdiplus::GdiplusStartupInput input;
  ULONG_PTR token{};
  if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok) return 1;
  int result;
  {
    Pet app(instance, std::wstring_view(arguments) != L"--bridge");
    result = app.run();
  }
  Gdiplus::GdiplusShutdown(token);
  return result;
}
