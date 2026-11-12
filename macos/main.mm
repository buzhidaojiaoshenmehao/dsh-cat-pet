#import <AppKit/AppKit.h>
#import "panel.hpp"

#include "animation.hpp"
#include "bubble.hpp"
#include "sounds.hpp"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <deque>
#include <random>
#include <unistd.h>

namespace {
using namespace paw;

constexpr CGFloat actorWidth = 292, actorHeight = 246, footY = 226;

double uptime() { return NSProcessInfo.processInfo.systemUptime; }

paw::Point pointer() {
  const NSPoint location = NSEvent.mouseLocation;
  return {location.x, -location.y};
}

paw::Rect rect(NSRect value) { return {NSMinX(value), -NSMaxY(value), NSMaxX(value), -NSMinY(value)}; }

NSURL* assets() {
  NSURL* bundle = NSBundle.mainBundle.bundleURL;
  return [bundle.URLByDeletingLastPathComponent.URLByDeletingLastPathComponent
    URLByAppendingPathComponent:@"assets" isDirectory:YES];
}

NSSound* chime(std::initializer_list<Note> notes) {
  const auto wave = makeChime(notes);
  return [[NSSound alloc] initWithData:[NSData dataWithBytes:wave.data() length:wave.size()]];
}

}

@interface PawController : NSObject <NSApplicationDelegate, NSMenuDelegate>
- (instancetype)initWithBridge:(BOOL)bridge;
- (void)press;
- (void)drag;
- (void)releasePointer;
- (void)showMenu:(NSEvent*)event inView:(NSView*)view;
@end

@interface PawWindow : NSPanel
@end

@implementation PawWindow
- (BOOL)canBecomeKeyWindow { return NO; }
- (BOOL)canBecomeMainWindow { return NO; }
- (NSRect)constrainFrameRect:(NSRect)frame toScreen:(NSScreen*)screen {
  (void)screen;
  return frame;
}
@end

@interface PawView : NSView
@property(weak) PawController* controller;
- (BOOL)loadSprites;
- (BOOL)hitAt:(NSPoint)point;
- (void)showPose:(Pose)pose scale:(CGFloat)scale title:(NSString*)title detail:(NSString*)detail
  hearts:(double)hearts ring:(NSPoint)ring ringAge:(double)ringAge;
@end

@implementation PawView {
  NSBitmapImageRep* _bitmaps[spriteCount];
  NSImage* _sprites[spriteCount];
  Pose _pose;
  NSRect _spriteRect;
  CGFloat _scale;
  NSString* _title;
  NSString* _detail;
  double _hearts, _ringAge;
  NSPoint _ring;
}

- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstMouse:(NSEvent*)event { (void)event; return YES; }
- (void)mouseDown:(NSEvent*)event { (void)event; [self.controller press]; }
- (void)mouseDragged:(NSEvent*)event { (void)event; [self.controller drag]; }
- (void)mouseUp:(NSEvent*)event { (void)event; [self.controller releasePointer]; }
- (void)rightMouseDown:(NSEvent*)event { [self.controller showMenu:event inView:self]; }

- (BOOL)loadSprites {
  NSURL* directory = assets();
  for (int i = 0; i < spriteCount; ++i) {
    NSURL* url = [directory URLByAppendingPathComponent:[NSString stringWithUTF8String:spriteFile(i).c_str()]];
    NSData* data = [NSData dataWithContentsOfURL:url];
    if (!data) return NO;
    _bitmaps[i] = [[NSBitmapImageRep alloc] initWithData:data];
    if (!_bitmaps[i] || _bitmaps[i].pixelsWide < 1 || _bitmaps[i].pixelsHigh < 1) return NO;
    _sprites[i] = [[NSImage alloc] initWithSize:_bitmaps[i].size];
    [_sprites[i] addRepresentation:_bitmaps[i]];
  }
  return YES;
}

- (BOOL)hitAt:(NSPoint)point {
  point.x /= _scale;
  point.y /= _scale;
  if (!NSPointInRect(point, _spriteRect)) return NO;
  NSBitmapImageRep* bitmap = _bitmaps[_pose.sprite];
  const CGFloat x = (point.x - NSMinX(_spriteRect)) / NSWidth(_spriteRect);
  const CGFloat y = (point.y - NSMinY(_spriteRect)) / NSHeight(_spriteRect);
  NSInteger pixelX = std::min(bitmap.pixelsWide - 1, NSInteger(x * bitmap.pixelsWide));
  if (_pose.mirrored) pixelX = bitmap.pixelsWide - 1 - pixelX;
  const NSInteger pixelY = std::min(bitmap.pixelsHigh - 1, NSInteger(y * bitmap.pixelsHigh));
  return [bitmap colorAtX:pixelX y:pixelY].alphaComponent >= .11;
}

- (void)showPose:(Pose)pose scale:(CGFloat)scale title:(NSString*)title detail:(NSString*)detail
  hearts:(double)hearts ring:(NSPoint)ring ringAge:(double)ringAge {
  _pose = pose;
  _scale = scale;
  _title = title;
  _detail = detail;
  _hearts = hearts;
  _ring = ring;
  _ringAge = ringAge;
  const NSSize image = _sprites[pose.sprite].size;
  const auto bounds = spriteBounds(image.width, image.height, pose, actorWidth / 2, footY);
  _spriteRect = NSMakeRect(bounds.left, bounds.top, bounds.width(), bounds.height());
  self.needsDisplay = YES;
}

- (void)drawPose {
  NSRect bounds = _spriteRect;
  const Pose& pose = _pose;
  [NSGraphicsContext saveGraphicsState];
  if (pose.mirrored) {
    NSAffineTransform* flip = [NSAffineTransform transform];
    [flip translateXBy:NSMaxX(bounds) yBy:NSMinY(bounds)];
    [flip scaleXBy:-1 yBy:1];
    [flip concat];
    bounds.origin = NSZeroPoint;
  }
  NSImage* image = _sprites[pose.sprite];
  [image drawInRect:bounds fromRect:NSZeroRect operation:NSCompositingOperationSourceOver
    fraction:1 respectFlipped:YES hints:nil];

  if (pose.blink > 0) {
    for (const auto& eye : eyesFor(pose.sprite)) {
      const double x = NSMinX(bounds) + eye.x * NSWidth(bounds), y = NSMinY(bounds) + eye.y * NSHeight(bounds);
      const double rx = eye.radiusX * NSWidth(bounds), ry = eye.radiusY * NSHeight(bounds);
      const NSRect lid = NSMakeRect(x - rx, y - ry, 2 * rx, 2 * ry);
      [NSGraphicsContext saveGraphicsState];
      [[NSBezierPath bezierPathWithOvalInRect:lid] addClip];
      NSRectClip(NSMakeRect(NSMinX(lid), NSMinY(lid), NSWidth(lid), NSHeight(lid) * pose.blink));
      [image drawInRect:NSOffsetRect(bounds, 0, 2.2 * ry) fromRect:NSZeroRect
        operation:NSCompositingOperationSourceOver fraction:1 respectFlipped:YES hints:nil];
      [NSGraphicsContext restoreGraphicsState];
      if (pose.blink > .72) {
        NSBezierPath* seam = [NSBezierPath bezierPath];
        [seam moveToPoint:NSMakePoint(x - .8 * rx, y)];
        [seam curveToPoint:NSMakePoint(x + .8 * rx, y)
          controlPoint1:NSMakePoint(x - .3 * rx, y + .32 * ry) controlPoint2:NSMakePoint(x + .3 * rx, y + .32 * ry)];
        [[NSColor colorWithWhite:.13 alpha:.7 * smoothStep((pose.blink - .72) / .28)] setStroke];
        seam.lineWidth = .8;
        [seam stroke];
      }
    }
  }
  [NSGraphicsContext restoreGraphicsState];
}

- (void)drawRect:(NSRect)dirtyRect {
  [NSColor.clearColor setFill];
  NSRectFillUsingOperation(dirtyRect, NSCompositingOperationCopy);
  [NSGraphicsContext saveGraphicsState];
  NSAffineTransform* zoom = [NSAffineTransform transform];
  [zoom scaleBy:_scale];
  [zoom concat];
  [NSGraphicsContext currentContext].imageInterpolation = NSImageInterpolationHigh;

  [[NSColor colorWithWhite:.15 alpha:.17] setFill];
  [[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(actorWidth / 2 - 59, footY - 6, 118, 12)] fill];
  [self drawPose];

  if (_title.length) {
    const BOOL receipt = _detail.length > 0;
    NSRect bubble = receipt ? NSMakeRect(18, 10, 256, 56) : NSMakeRect(75, 39, 142, 27);
    [[NSColor colorWithCalibratedRed:1 green:.985 blue:.96 alpha:.97] setFill];
    [[NSBezierPath bezierPathWithRoundedRect:bubble xRadius:12 yRadius:12] fill];
    NSColor* ink = [NSColor colorWithCalibratedRed:.16 green:.25 blue:.22 alpha:1];
    [_title drawInRect:NSInsetRect(bubble, 11, 6) withAttributes:@{
      NSFontAttributeName: [NSFont systemFontOfSize:receipt ? 13 : 11 weight:NSFontWeightSemibold],
      NSForegroundColorAttributeName: ink,
    }];
    if (receipt) [_detail drawInRect:NSMakeRect(29, 38, 234, 20) withAttributes:@{
      NSFontAttributeName: [NSFont monospacedDigitSystemFontOfSize:11 weight:NSFontWeightMedium],
      NSForegroundColorAttributeName: ink,
    }];
  }
  if (_hearts >= 0 && _hearts < 1.25) {
    NSColor* pink = [NSColor colorWithCalibratedRed:.87 green:.34 blue:.44 alpha:1 - _hearts / 1.25];
    for (int i = 0; i < 3; ++i) [@"♥" drawAtPoint:NSMakePoint(193 + i * 15, 120 - _hearts * 46 - i * 8)
      withAttributes:@{NSFontAttributeName: [NSFont systemFontOfSize:17 - i * 2], NSForegroundColorAttributeName:pink}];
  }
  if (_ringAge >= 0 && _ringAge < .48) {
    const CGFloat t = _ringAge / .48, radius = 5 + t * 11;
    [[NSColor colorWithCalibratedRed:.86 green:.57 blue:.29 alpha:.85 * (1 - t)] setStroke];
    NSBezierPath* ring = [NSBezierPath bezierPathWithOvalInRect:
      NSMakeRect(_ring.x - radius, _ring.y - radius, radius * 2, radius * 2)];
    ring.lineWidth = 2;
    [ring stroke];
  }
  [NSGraphicsContext restoreGraphicsState];
}
@end

@implementation PawController {
  BOOL _bridge, _linked, _hidden, _pressed, _dragged, _roam, _chase, _menuOpen, _sound, _facingRight, _walking, _rub;
  PawWindow* _window;
  PawView* _view;
  PawPanel* _panel;
  NSStatusItem* _status;
  NSMenu* _menu;
  NSMenuItem* _visibilityItem;
  NSMenuItem* _roamItem;
  NSMenuItem* _chaseItem;
  NSMenuItem* _soundItem;
  NSUserDefaults* _settings;
  NSTimer* _timer;
  dispatch_source_t _input;
  NSSound* _meowSound;
  NSSound* _finishSound;
  NSSound* _playingSound;
  NSInteger _size, _demoCycle;
  paw::Point _position, _lastPointer, _pressPoint, _pressOffset, _catchPoint;
  std::vector<Screen> _screens;
  std::deque<paw::Point> _route;
  std::mt19937 _random;
  size_t _roamCount;
  double _start, _lastTick, _idleSince, _nextRoam, _walked, _walkSpeed, _hoverSince, _pointerMoved;
  double _playUntil, _playedAt, _caughtAt, _chaseAfter, _nextMeow, _toastUntil, _stretchAt;
  PoseAnimator _poses;
  BubbleLayout _bubbleLayout;
  BubbleHover _bubbleHover;
  int64_t _clockSecond;
  std::string _incoming, _activity, _count;
  Balance _balance;
  std::optional<Task> _recent, _toast;
  std::deque<Task> _queued;
}

- (instancetype)initWithBridge:(BOOL)bridge {
  self = [super init];
  if (self) {
    _bridge = bridge;
    _roam = _chase = YES;
    _activity = "idle";
    _count = "0";
    _demoCycle = -1;
    _random.seed(std::random_device{}());
  }
  return self;
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
  (void)notification;
  _settings = NSUserDefaults.standardUserDefaults;
  _size = [_settings integerForKey:@"sizePercent"];
  if (_size < 50 || _size > 180) _size = 100;
  _sound = [_settings boolForKey:@"soundEnabled"];
  _view = [[PawView alloc] initWithFrame:NSMakeRect(0, 0, actorWidth, actorHeight)];
  _view.controller = self;
  [self collectScreens];
  if (_screens.empty() || ![_view loadSprites]) {
    fputs("Little Paw needs a macOS desktop session and its PNG assets.\n", stderr);
    [NSApp terminate:nil];
    return;
  }
  _window = [[PawWindow alloc] initWithContentRect:_view.frame
    styleMask:NSWindowStyleMaskBorderless | NSWindowStyleMaskNonactivatingPanel
    backing:NSBackingStoreBuffered defer:NO];
  _window.title = @"小爪同伴 · 桌宠";
  _window.opaque = NO;
  _window.backgroundColor = NSColor.clearColor;
  _window.hasShadow = NO;
  _window.hidesOnDeactivate = NO;
  _window.level = NSFloatingWindowLevel;
  _window.ignoresMouseEvents = YES;
  _window.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces |
    NSWindowCollectionBehaviorFullScreenAuxiliary | NSWindowCollectionBehaviorIgnoresCycle;
  _window.contentView = _view;
  [self setupMenu];
  _lastPointer = pointer();
  const paw::Rect work = _screens[nearestScreen(_screens, _lastPointer)].work;
  _position = safeArea(work, [self scale]).clamp({work.right - 148 * [self scale], work.bottom - 34});
  _start = _lastTick = _idleSince = uptime();
  _nextRoam = _start + 2.1;
  [self paint:_start];
  [_window orderFrontRegardless];
  _timer = [NSTimer timerWithTimeInterval:1.0 / 60 target:self selector:@selector(tick:) userInfo:nil repeats:YES];
  [NSRunLoop.mainRunLoop addTimer:_timer forMode:NSRunLoopCommonModes];
  if (_bridge) {
    _input = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, STDIN_FILENO, 0, dispatch_get_main_queue());
    __weak PawController* weakSelf = self;
    dispatch_source_set_event_handler(_input, ^{ [weakSelf readPipe]; });
    dispatch_resume(_input);
  }
}

- (void)applicationWillTerminate:(NSNotification*)notification {
  (void)notification;
  [_timer invalidate];
  if (_input) dispatch_source_cancel(_input);
  [_playingSound stop];
  if (_status) [NSStatusBar.systemStatusBar removeStatusItem:_status];
}

- (void)applicationDidChangeScreenParameters:(NSNotification*)notification {
  (void)notification;
  [self collectScreens];
  _route.clear();
  if (!_screens.empty()) _position = safeArea(_screens[nearestScreen(_screens, _position)].work, [self scale]).clamp(_position);
  if (_panel.visible) [self placePanel];
}

- (void)collectScreens {
  _screens.clear();
  for (NSScreen* screen in NSScreen.screens) _screens.push_back({rect(screen.frame), rect(screen.visibleFrame)});
}

- (CGFloat)scale { return _size / 100.0; }

- (void)readPipe {
  char bytes[4096];
  const auto count = read(STDIN_FILENO, bytes, sizeof(bytes));
  if (count < 0 && (errno == EINTR || errno == EAGAIN)) return;
  if (count <= 0) { [NSApp terminate:nil]; return; }
  _incoming.append(bytes, count);
  size_t end;
  while ((end = _incoming.find('\n')) != std::string::npos) {
    if (end > 4096) { [NSApp terminate:nil]; return; }
    std::string line = _incoming.substr(0, end);
    _incoming.erase(0, end + 1);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (![self receive:line]) { [NSApp terminate:nil]; return; }
  }
  if (_incoming.size() > 4096) [NSApp terminate:nil];
}

- (BOOL)receive:(const std::string&)line {
  const auto packet = fields(line);
  if (packet[0] == "quit") return NO;
  if (packet[0] == "hello") {
    _linked = packet.size() == 2 && packet[1] == "1";
    [self updatePanel];
    return _linked;
  }
  if (!_linked) return YES;
  if (packet[0] == "activity" && packet.size() == 3) {
    [self setActivity:packet[1] count:packet[2]];
  } else if (packet[0] == "task" || packet[0] == "recent") {
    if (const auto task = parseTask(packet)) [self acceptTask:*task announce:packet[0] == "task"];
  } else if (packet[0] == "balance") {
    if (auto next = parseBalance(packet)) {
      if (next->status == "loading") _balance.status = "loading";
      else _balance = std::move(*next);
      [self updatePanel];
    }
  }
  return YES;
}

- (void)setActivity:(const std::string&)value count:(const std::string&)count {
  if (_activity == value && _count == count) return;
  _activity = value;
  _count = count;
  const double now = uptime();
  if (_activity == "idle") _idleSince = now;
  if (_activity == "working") _nextRoam = std::min(_nextRoam, now + .9);
  [self updatePanel];
}

- (NSString*)activityText {
  if (_activity == "thinking") return @"思考中";
  if (_activity == "working") return @"执行工具";
  return !_bridge || _linked ? @"待命" : @"连接中";
}

- (void)acceptTask:(const Task&)task announce:(BOOL)announce {
  _recent = task;
  if (announce && !_hidden) {
    if (_queued.size() == 16) _queued.pop_front();
    _queued.push_back(task);
  }
  [self updatePanel];
}

- (void)advanceDemo:(double)now {
  const double elapsed = now - _start;
  const double phase = std::fmod(elapsed, 26);
  if (phase >= 3.1 && phase < 6.7) [self setActivity:"thinking" count:"1"];
  else if (phase >= 6.7 && phase < 11.6) [self setActivity:"working" count:"1"];
  else [self setActivity:"idle" count:"0"];
  const NSInteger cycle = NSInteger(elapsed / 26);
  if (phase >= 11.6 && _demoCycle != cycle) {
    _demoCycle = cycle;
    [self acceptTask:Task{"completed", "main", "4680", "4200", "480", "3200", "0.002984", "0.000448", "full", "api"}
      announce:YES];
  }
}

- (void)tick:(NSTimer*)timer {
  (void)timer;
  const double now = uptime(), dt = std::min(.06, now - _lastTick);
  _lastTick = now;
  if (!_bridge) [self advanceDemo:now];
  if (_panel.visible) {
    const bool inside = inBubble(_bubbleLayout, pointer());
    _panel.ignoresMouseEvents = !inside;
    if (_bubbleHover.expired(now, inside)) [_panel orderOut:nil];
    const auto second = int64_t(NSDate.date.timeIntervalSince1970);
    if (_clockSecond != second) {
      _clockSecond = second;
      [_panel refreshClock];
    }
  }
  if (_hidden || _screens.empty()) return;
  if (_toast && now >= _toastUntil) _toast.reset();
  if (!_toast && !_queued.empty()) {
    _toast = _queued.front();
    _queued.pop_front();
    _toastUntil = now + 4.6;
    if (_toast->outcome == "completed") [self playFinishSound];
  }
  [self moveAt:now delta:dt];
  if (!_walking) _walkSpeed = 0;
  [self paint:now];
}

- (BOOL)onCat:(paw::Point)mouse {
  NSPoint point = [_window convertPointFromScreen:NSMakePoint(mouse.x, -mouse.y)];
  return [_view hitAt:[_view convertPoint:point fromView:nil]];
}

- (void)moveAt:(double)now delta:(double)dt {
  _walking = NO;
  const paw::Point mouse = pointer();
  if (length(mouse - _lastPointer) >= 2) _pointerMoved = now;
  _lastPointer = mouse;
  if (_menuOpen || _panel.visible || _pressed) return;
  if ([self onCat:mouse]) {
    if (!_hoverSince) {
      _hoverSince = now;
      if (now >= _playUntil) [self play:NO];
    } else if (!_rub && now - _hoverSince >= 1 && now - _hoverSince < 1.8) [self play:YES];
    return;
  }
  _hoverSince = 0;
  if (_toast || now < _playUntil || (_stretchAt && now - _stretchAt < stretchDuration)) return;
  const double scale = [self scale];
  const paw::Point center = _position - paw::Point{0, 60 * scale};
  const double distance = length(mouse - center);
  if (_chase && now - _pointerMoved < .65 && distance < 170 * scale && now >= _chaseAfter) {
    const paw::Rect area = safeArea(_screens[nearestScreen(_screens, _position)].work, scale);
    const double side = mouse.x > center.x ? -1 : 1;
    const paw::Point goal = area.clamp({mouse.x + side * 37 * scale, mouse.y + 64 * scale});
    if (distance < 63 * scale || length(goal - _position) < 9 * scale) {
      _facingRight = mouse.x > center.x;
      [self play:NO];
      [self meow:NO];
      _caughtAt = now;
      _catchPoint = mouse;
      _chaseAfter = now + 2.1;
    } else {
      _route.clear();
      [self stepToward:goal delta:dt speedLimit:215 * scale arriving:YES];
    }
    return;
  }
  if (!_roam) return;
  if (_route.empty() && now >= _nextRoam) [self chooseRoute];
  while (!_route.empty() && length(_route.front() - _position) <= 3 * scale) {
    _position = _route.front();
    _route.pop_front();
    if (_route.empty()) {
      _nextRoam = now + (_activity == "working" ? 1.9 : 4.7);
      if (_activity == "idle" && _roamCount % 3 == 0) _stretchAt = now;
    }
  }
  if (!_route.empty()) [self stepToward:_route.front() delta:dt speedLimit:(_activity == "working" ? 116 : 88) * scale
    arriving:_route.size() == 1];
}

- (void)chooseRoute {
  const size_t from = nearestScreen(_screens, _position);
  const size_t to = ++_roamCount % 3 == 0 ? (from + 1) % _screens.size() : from;
  const double scale = [self scale];
  paw::Rect area = safeArea(_screens[to].work, scale);
  if (_activity == "working" && from == to && area.contains(_position)) {
    area = {std::max(area.left, _position.x - 230 * scale), std::max(area.top, _position.y - 120 * scale),
      std::min(area.right, _position.x + 230 * scale), std::min(area.bottom, _position.y + 120 * scale)};
  }
  std::uniform_real_distribution<double> random(.12, .88);
  const paw::Point target{area.left + random(_random) * area.width(), area.top + random(_random) * area.height()};
  auto points = route(_screens, from, to, target);
  if (points.empty()) points.push_back(safeArea(_screens[from].work, scale).center());
  _route.assign(points.begin(), points.end());
}

- (void)stepToward:(paw::Point)goal delta:(double)dt speedLimit:(double)limit arriving:(BOOL)arriving {
  const paw::Point vector = goal - _position;
  const double remaining = length(vector);
  if (remaining < .1) return;
  const double step = walkingStep(remaining, dt, limit, arriving, _walkSpeed);
  if (std::abs(vector.x) > 3) _facingRight = vector.x > 0;
  _position = _position + vector * (step / remaining);
  _walked += step / [self scale];
  _walking = YES;
}

- (void)paint:(double)now {
  const double elapsed = now - _start;
  Action action = Action::sit;
  double age = 0;
  if (now < _playUntil) {
    action = _rub ? Action::roll : Action::reach;
    age = now - _playedAt;
  } else if (_toast) {
    action = _toast->outcome == "completed" ? Action::celebrate : Action::notice;
    age = 4.6 - (_toastUntil - now);
  } else if (_walking) action = Action::walk;
  else if (_stretchAt && now - _stretchAt < stretchDuration) {
    action = Action::stretch;
    age = now - _stretchAt;
  } else if (_activity == "working") action = Action::work;
  else if (_activity == "thinking") action = Action::look;
  else if (now - _idleSince > 17) action = Action::sleep;
  const auto framePose = _poses.sample(catPose(action, elapsed, age, _walked, _facingRight), now);

  const double scale = [self scale];
  const paw::Point topLeft = _position - paw::Point{actorWidth * scale / 2, footY * scale};
  const NSRect frame = NSMakeRect(topLeft.x, -topLeft.y - actorHeight * scale, actorWidth * scale, actorHeight * scale);
  const NSRect previous = _window.frame;
  if (!NSEqualSizes(previous.size, frame.size)) [_window setFrame:frame display:NO];
  else if (!NSEqualPoints(previous.origin, frame.origin)) [_window setFrameOrigin:frame.origin];
  NSString* title = nil;
  NSString* detail = nil;
  if (_toast) {
    title = pawText(taskTitle(*_toast));
    detail = pawText(tokenText(*_toast) + L"  ·  " + taskPrice(*_toast, _balance));
  } else if (_activity != "idle") {
    title = _count == "1" ? [self activityText] :
      [[self activityText] stringByAppendingFormat:@" · %@", pawText(grouped(_count))];
  }
  const paw::Point ring = (_catchPoint - topLeft) * (1 / scale);
  [_view showPose:framePose scale:scale title:title detail:detail
    hearts:_playedAt ? now - _playedAt : -1 ring:NSMakePoint(ring.x, ring.y) ringAge:_caughtAt ? now - _caughtAt : -1];
  _window.ignoresMouseEvents = !_pressed && ![self onCat:pointer()];
}

- (void)press {
  _pressed = YES;
  _dragged = NO;
  _pressPoint = pointer();
  _pressOffset = _position - _pressPoint;
  _route.clear();
}

- (void)drag {
  if (!_pressed || _screens.empty()) return;
  const paw::Point mouse = pointer();
  if (length(mouse - _pressPoint) >= 6 * [self scale]) _dragged = YES;
  if (_dragged) _position = safeArea(_screens[nearestScreen(_screens, mouse)].work, [self scale]).clamp(mouse + _pressOffset);
}

- (void)releasePointer {
  const BOOL click = _pressed && !_dragged && length(pointer() - _pressPoint) < 6 * [self scale];
  _pressed = NO;
  if (click) {
    [self meow:NO];
    [self openPanel:nil];
  }
}

- (void)play:(BOOL)rub {
  const double now = uptime();
  _rub = rub;
  _stretchAt = 0;
  _playedAt = now;
  _playUntil = now + (rub ? 1.5 : .95);
  _route.clear();
  _nextRoam = std::max(_nextRoam, _playUntil + 2.2);
  if (rub) [self meow:NO];
}

- (void)meow:(BOOL)preview {
  const double now = uptime();
  if (!_sound || (!preview && now < _nextMeow)) return;
  if (!_meowSound) _meowSound = [[NSSound alloc] initWithContentsOfURL:[assets() URLByAppendingPathComponent:@"meow.wav"]
    byReference:NO];
  if (!_meowSound) return;
  _nextMeow = now + 5;
  [_playingSound stop];
  _playingSound = _meowSound;
  [_playingSound play];
}

- (void)playFinishSound {
  if (!_sound) return;
  if (!_finishSound) _finishSound = chime({{523.25, 88}, {659.25, 88}, {783.99, 195}});
  _nextMeow = std::max(_nextMeow, uptime() + .5);
  [_playingSound stop];
  _playingSound = _finishSound;
  [_playingSound play];
}

- (void)placePanel {
  if (_screens.empty()) return;
  const paw::Rect work = _screens[nearestScreen(_screens, _position)].work;
  _bubbleLayout = placeBubble(work, _position, pawPanelWidth, pawPanelHeight, 1, [self scale]);
  [_panel setFrameTopLeftPoint:NSMakePoint(_bubbleLayout.frame.left, -_bubbleLayout.frame.top)];
  [_panel setTailOnLeft:_bubbleLayout.tailLeft offset:_bubbleLayout.tailY];
}

- (void)openPanel:(id)sender {
  (void)sender;
  if (_hidden) [self toggleVisible:nil];
  _route.clear();
  if (!_panel) _panel = [[PawPanel alloc] initWithTarget:self refresh:@selector(refreshBalance:)
    resize:@selector(resize:) sound:@selector(toggleSound:) menu:_menu];
  [self placePanel];
  _bubbleHover.opened(uptime());
  _panel.ignoresMouseEvents = !inBubble(_bubbleLayout, pointer());
  [self updatePanel];
  [_panel refreshClock];
  [_panel makeKeyAndOrderFront:nil];
  [self refreshBalance:nil];
}

- (void)updatePanel {
  [_panel setActivityText:[NSString stringWithFormat:@"%@  ·  %@", _bridge ? @"DeepSeek Harness" : @"演示数据",
    [self activityText]]];
  [_panel showBalance:_balance];
  [_panel showTask:_recent balance:_balance];
  [_panel showSize:_size sound:_sound];
}

- (void)refreshBalance:(id)sender {
  (void)sender;
  if (_bridge && _balance.status != "loading") {
    _balance.status = "loading";
    if (write(STDOUT_FILENO, "balance\n", 8) != 8) { [NSApp terminate:nil]; return; }
  } else if (!_bridge) _balance = {"ready", "api", "1", {{"CNY", "28.60", "3.60", "25.00"}}};
  [self updatePanel];
}

- (void)resize:(NSSlider*)sender {
  const NSInteger size = std::clamp(sender.integerValue, NSInteger(50), NSInteger(180));
  if (_size == size) return;
  _size = size;
  [_settings setInteger:_size forKey:@"sizePercent"];
  _route.clear();
  if (!_screens.empty()) _position = safeArea(_screens[nearestScreen(_screens, _position)].work, [self scale]).clamp(_position);
  [_panel showSize:_size sound:_sound];
}

- (void)toggleSound:(id)sender {
  (void)sender;
  _sound = !_sound;
  [_settings setBool:_sound forKey:@"soundEnabled"];
  [_panel showSize:_size sound:_sound];
  if (_sound) [self meow:YES];
  else [_playingSound stop];
}

- (NSMenuItem*)addItem:(NSString*)title action:(SEL)action {
  NSMenuItem* item = [_menu addItemWithTitle:title action:action keyEquivalent:@""];
  item.target = self;
  return item;
}

- (void)setupMenu {
  _status = [NSStatusBar.systemStatusBar statusItemWithLength:NSSquareStatusItemLength];
  _status.autosaveName = @"Little Paw";
  _status.visible = YES;
  NSStatusBarButton* button = _status.button;
  NSImage* symbol = [NSImage imageWithSystemSymbolName:@"pawprint.fill" accessibilityDescription:@"小爪同伴"];
  if (symbol) {
    button.image = [symbol imageWithSymbolConfiguration:[NSImageSymbolConfiguration
      configurationWithPointSize:15 weight:NSFontWeightSemibold]] ?: symbol;
    [button.image setTemplate:YES];
    button.imagePosition = NSImageOnly;
  } else button.title = @"🐾";
  [button setAccessibilityLabel:@"小爪同伴"];
  button.toolTip = _bridge ? @"小爪同伴 · DeepSeek Harness" : @"小爪同伴 · 演示";
  _menu = [[NSMenu alloc] initWithTitle:@"小爪同伴"];
  _menu.delegate = self;
  [self addItem:@"余额、最近任务与设置" action:@selector(openPanel:)];
  [self addItem:@"摸摸猫" action:@selector(petCat:)];
  [self addItem:@"伸懒腰" action:@selector(stretch:)];
  [_menu addItem:NSMenuItem.separatorItem];
  _roamItem = [self addItem:@"桌面巡游" action:@selector(toggleRoam:)];
  _chaseItem = [self addItem:@"追光标" action:@selector(toggleChase:)];
  _soundItem = [self addItem:@"音效" action:@selector(toggleSound:)];
  [self addItem:@"叫到当前屏幕" action:@selector(bringHere:)];
  _visibilityItem = [self addItem:@"隐藏桌宠" action:@selector(toggleVisible:)];
  [_menu addItem:NSMenuItem.separatorItem];
  [self addItem:@"退出桌宠" action:@selector(quit:)];
  _status.menu = _menu;
}

- (void)showMenu:(NSEvent*)event inView:(NSView*)view { [NSMenu popUpContextMenu:_menu withEvent:event forView:view]; }
- (void)menuWillOpen:(NSMenu*)menu {
  (void)menu;
  _menuOpen = YES;
  _roamItem.state = _roam ? NSControlStateValueOn : NSControlStateValueOff;
  _chaseItem.state = _chase ? NSControlStateValueOn : NSControlStateValueOff;
  _soundItem.state = _sound ? NSControlStateValueOn : NSControlStateValueOff;
  _visibilityItem.title = _hidden ? @"显示桌宠" : @"隐藏桌宠";
}
- (void)menuDidClose:(NSMenu*)menu { (void)menu; _menuOpen = NO; }
- (void)toggleRoam:(id)sender { (void)sender; _roam = !_roam; _route.clear(); }
- (void)toggleChase:(id)sender { (void)sender; _chase = !_chase; }
- (void)petCat:(id)sender { (void)sender; if (_hidden) [self toggleVisible:nil]; [self play:YES]; }

- (void)stretch:(id)sender {
  (void)sender;
  if (_hidden) [self toggleVisible:nil];
  _route.clear();
  _playUntil = 0;
  _stretchAt = uptime();
  _nextRoam = std::max(_nextRoam, _stretchAt + 2.2);
}

- (void)bringHere:(id)sender {
  (void)sender;
  if (_screens.empty()) return;
  const paw::Point mouse = pointer();
  _route.clear();
  _position = safeArea(_screens[nearestScreen(_screens, mouse)].work, [self scale]).clamp(mouse + paw::Point{88, 95});
  if (_hidden) [self toggleVisible:nil];
  [self paint:uptime()];
}

- (void)toggleVisible:(id)sender {
  (void)sender;
  _hidden = !_hidden;
  if (_hidden) {
    [_window orderOut:nil];
    [_panel orderOut:nil];
    _toast.reset();
    _queued.clear();
    _pressed = NO;
    [_playingSound stop];
  } else {
    [_window orderFrontRegardless];
  }
}

- (void)quit:(id)sender {
  (void)sender;
  if (_bridge) (void)write(STDOUT_FILENO, "exit\n", 5);
  [NSApp terminate:nil];
}
@end

int main(int argc, const char* argv[]) {
  @autoreleasepool {
    std::signal(SIGPIPE, SIG_IGN);
    NSApplication* app = NSApplication.sharedApplication;
    [app setActivationPolicy:NSApplicationActivationPolicyAccessory];
    __attribute__((objc_precise_lifetime)) PawController* controller =
      [[PawController alloc] initWithBridge:argc == 2 && std::string_view(argv[1]) == "--bridge"];
    app.delegate = controller;
    [app run];
  }
}
