#import "panel.hpp"

#include "bubble.hpp"
#include "tariff.hpp"
#include "pricing-calendar.hpp"

namespace {

NSColor* panelColor(uint32_t rgb) {
  return [NSColor colorWithSRGBRed:((rgb >> 16) & 255) / 255.0 green:((rgb >> 8) & 255) / 255.0
    blue:(rgb & 255) / 255.0 alpha:1];
}

}

@interface FlippedContent : NSView
@end

@implementation FlippedContent
- (BOOL)isFlipped { return YES; }
@end

@interface BubbleContent : FlippedContent
@property BOOL tailOnLeft;
@property CGFloat tailY;
@end

@implementation BubbleContent
- (void)drawRect:(NSRect)dirty {
  [NSColor.clearColor setFill];
  NSRectFillUsingOperation(dirty, NSCompositingOperationCopy);
  const CGFloat gutter = paw::bubbleGutter;
  const NSRect body = NSInsetRect(self.bounds, gutter, 0);
  NSBezierPath* outline = [NSBezierPath bezierPathWithRoundedRect:body
    xRadius:paw::bubbleRadius yRadius:paw::bubbleRadius];
  const CGFloat edge = self.tailOnLeft ? NSMinX(body) + 1 : NSMaxX(body) - 1;
  const CGFloat tip = self.tailOnLeft ? NSMinX(self.bounds) + 1 : NSMaxX(self.bounds) - 1;
  NSBezierPath* tail = [NSBezierPath bezierPath];
  [tail moveToPoint:NSMakePoint(edge, self.tailY - paw::bubbleTailHalfHeight)];
  [tail lineToPoint:NSMakePoint(tip, self.tailY)];
  [tail lineToPoint:NSMakePoint(edge, self.tailY + paw::bubbleTailHalfHeight)];
  [tail closePath];
  [panelColor(paw::bubbleFill) setFill];
  [outline fill];
  [tail fill];
}
@end

namespace {

NSTextField* textLine(NSView* view, NSRect frame, CGFloat size, NSFontWeight weight, NSColor* color) {
  NSTextField* label = [NSTextField labelWithString:@""];
  label.frame = frame;
  label.font = [NSFont systemFontOfSize:size weight:weight];
  label.textColor = color;
  label.lineBreakMode = NSLineBreakByTruncatingTail;
  [view addSubview:label];
  return label;
}

NSBox* surface(NSView* view, NSRect frame, CGFloat radius, uint32_t fill) {
  NSBox* box = [[NSBox alloc] initWithFrame:frame];
  box.boxType = NSBoxCustom;
  box.titlePosition = NSNoTitle;
  box.borderWidth = 0;
  box.fillColor = panelColor(fill);
  box.cornerRadius = radius;
  [view addSubview:box];
  return box;
}

NSImage* symbol(NSString* name, CGFloat size) {
  return [[NSImage imageWithSystemSymbolName:name accessibilityDescription:nil]
    imageWithSymbolConfiguration:[NSImageSymbolConfiguration configurationWithPointSize:size weight:NSFontWeightSemibold]];
}

NSButton* iconButton(NSView* view, NSRect frame, NSString* name, NSString* fallback, NSString* label, id target, SEL action) {
  NSButton* button = [NSButton buttonWithTitle:fallback target:target action:action];
  button.image = symbol(name, 13);
  if (button.image) button.imagePosition = NSImageOnly;
  button.contentTintColor = panelColor(0x246454);
  button.bordered = NO;
  button.frame = frame;
  [button setAccessibilityLabel:label];
  button.toolTip = label;
  [view addSubview:button];
  return button;
}

NSSwitch* toggle(NSView* view, NSRect frame, NSString* label, id target, SEL action) {
  NSSwitch* control = [[NSSwitch alloc] initWithFrame:frame];
  control.controlSize = NSControlSizeSmall;
  control.target = target;
  control.action = action;
  [control setAccessibilityLabel:label];
  [view addSubview:control];
  return control;
}

NSString* duration(int64_t seconds) {
  const auto days = seconds / 86400;
  NSString* clock = [NSString stringWithFormat:@"%02d:%02d:%02d", int(seconds / 3600 % 24),
    int(seconds / 60 % 60), int(seconds % 60)];
  return days ? [NSString stringWithFormat:@"%lld 天 %@", static_cast<long long>(days), clock] : clock;
}

}

@implementation PawPanel {
  BubbleContent* _bubble;
  NSMenu* _actions;
  NSTextField* _activity;
  NSTextField* _amount[2];
  NSTextField* _walletBonus[2];
  NSTextField* _walletCash[2];
  NSTextField* _balanceNotice;
  NSButton* _refresh;
  NSBox* _periodBadge;
  NSTextField* _period;
  NSTextField* _remaining;
  NSTextField* _deadline;
  NSTextField* _taskUsage;
  NSTextField* _taskPrice;
  NSTextField* _taskBreakdown;
  NSTextField* _taskCache;
  NSTextField* _taskNote;
  NSButton* _disclosure;
  FlippedContent* _settingsView;
  NSTextField* _sizeLabel;
  NSSlider* _size;
  NSSwitch* _sound;
  NSSwitch* _roam;
  NSDateFormatter* _beijing;
  BOOL _taskDetailsExpanded;
}

- (instancetype)initWithTarget:(id)target refresh:(SEL)refresh resize:(SEL)resize
    sound:(SEL)sound roam:(SEL)roam details:(SEL)details menu:(NSMenu*)menu portrait:(NSImage*)portrait {
  const NSRect frame = NSMakeRect(0, 0, paw::bubbleWidth + 2 * paw::bubbleGutter, paw::bubbleHeight);
  self = [super initWithContentRect:frame
    styleMask:NSWindowStyleMaskBorderless | NSWindowStyleMaskNonactivatingPanel
    backing:NSBackingStoreBuffered defer:NO];
  if (!self) return nil;
  self.title = @"小爪同伴";
  self.releasedWhenClosed = NO;
  self.floatingPanel = YES;
  self.hidesOnDeactivate = NO;
  self.opaque = NO;
  self.backgroundColor = NSColor.clearColor;
  self.hasShadow = YES;
  self.appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
  self.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorFullScreenAuxiliary;
  _bubble = [[BubbleContent alloc] initWithFrame:frame];
  _bubble.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  _actions = menu;
  FlippedContent* view = [[FlippedContent alloc] initWithFrame:NSInsetRect(frame, paw::bubbleGutter, 0)];
  view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  [_bubble addSubview:view];
  self.contentView = _bubble;
  NSColor* ink = panelColor(0x2b2d28);
  NSColor* quiet = panelColor(0x655e55);
  NSColor* jade = panelColor(0x246454);

  surface(view, NSMakeRect(14, 9, 32, 32), 9, 0xf2e1d1);
  NSImageView* avatar = [[NSImageView alloc] initWithFrame:NSMakeRect(14, 9, 32, 32)];
  avatar.image = portrait;
  avatar.imageScaling = NSImageScaleProportionallyDown;
  [view addSubview:avatar];
  textLine(view, NSMakeRect(55, 14, 89, 23), 16, NSFontWeightSemibold, ink).stringValue = @"小爪同伴";
  _activity = textLine(view, NSMakeRect(145, 16, 81, 19), 11, NSFontWeightMedium, quiet);
  surface(view, NSMakeRect(229, 12, 26, 26), 7, paw::bubbleControlsFill);
  surface(view, NSMakeRect(260, 12, 26, 26), 7, paw::bubbleControlsFill);
  iconButton(view, NSMakeRect(229, 12, 26, 26), @"pawprint.fill", @"🐾", @"桌宠菜单", self, @selector(showActions:));
  iconButton(view, NSMakeRect(260, 12, 26, 26), @"xmark", @"×", @"关闭气泡", self, @selector(cancelOperation:));
  for (CGFloat y : {47, 132, 201}) surface(view, NSMakeRect(14, y, 272, 1), 0, paw::bubbleDivider);

  textLine(view, NSMakeRect(15, 55, 121, 19), 13, NSFontWeightSemibold, ink).stringValue = @"DeepSeek 余额";
  _balanceNotice = textLine(view, NSMakeRect(136, 57, 94, 17), 10, NSFontWeightMedium, panelColor(paw::bubblePeak));
  _balanceNotice.alignment = NSTextAlignmentRight;
  _refresh = [NSButton buttonWithTitle:@"刷新" target:target action:refresh];
  _refresh.frame = NSMakeRect(232, 50, 55, 27);
  _refresh.bezelStyle = NSBezelStyleRounded;
  _refresh.controlSize = NSControlSizeSmall;
  [_refresh setAccessibilityLabel:@"刷新余额"];
  [view addSubview:_refresh];
  for (int i = 0; i < 2; ++i) {
    const CGFloat x = 15 + 140 * i;
    _amount[i] = textLine(view, NSMakeRect(x, 76, 132, 29), 17, NSFontWeightSemibold, jade);
    _walletBonus[i] = textLine(view, NSMakeRect(x, 102, 132, 14), 10, NSFontWeightRegular, quiet);
    _walletCash[i] = textLine(view, NSMakeRect(x, 115, 132, 14), 10, NSFontWeightRegular, quiet);
  }

  textLine(view, NSMakeRect(15, 142, 88, 19), 13, NSFontWeightSemibold, ink).stringValue = @"峰谷倒计时";
  textLine(view, NSMakeRect(106, 144, 64, 16), 10, NSFontWeightRegular, quiet).stringValue = @"北京时间";
  _periodBadge = surface(view, NSMakeRect(207, 140, 79, 21), 10, paw::bubbleOffPeakFill);
  _period = textLine(view, NSMakeRect(207, 142, 79, 18), 11, NSFontWeightSemibold, quiet);
  _period.alignment = NSTextAlignmentCenter;
  _remaining = textLine(view, NSMakeRect(15, 163, 150, 28), 22, NSFontWeightSemibold, ink);
  _deadline = textLine(view, NSMakeRect(161, 171, 125, 18), 11, NSFontWeightRegular, quiet);
  _deadline.alignment = NSTextAlignmentRight;

  textLine(view, NSMakeRect(15, 208, 100, 18), 13, NSFontWeightSemibold, ink).stringValue = @"最近任务";
  _taskUsage = textLine(view, NSMakeRect(15, 227, 153, 20), 13, NSFontWeightMedium, ink);
  _taskPrice = textLine(view, NSMakeRect(167, 227, 97, 20), 12, NSFontWeightMedium, ink);
  _taskPrice.alignment = NSTextAlignmentRight;
  _disclosure = iconButton(view, NSMakeRect(263, 224, 25, 26), @"chevron.right", @"›", @"展开任务明细",
    target, details);
  _taskBreakdown = textLine(view, NSMakeRect(15, 253, 271, 16), 11, NSFontWeightRegular, quiet);
  _taskCache = textLine(view, NSMakeRect(15, 270, 271, 16), 11, NSFontWeightRegular, quiet);
  _taskNote = textLine(view, NSMakeRect(15, 287, 271, 16), 10, NSFontWeightRegular, quiet);

  _settingsView = [[FlippedContent alloc] initWithFrame:NSMakeRect(12, 252, 276, 65)];
  [view addSubview:_settingsView];
  surface(_settingsView, _settingsView.bounds, 10, paw::bubbleControlsFill);
  surface(_settingsView, NSMakeRect(8, 31, 260, 1), 0, paw::bubbleDivider);
  textLine(_settingsView, NSMakeRect(9, 8, 62, 19), 12, NSFontWeightSemibold, ink).stringValue = @"桌宠大小";
  _sizeLabel = textLine(_settingsView, NSMakeRect(223, 8, 43, 19), 12, NSFontWeightMedium, jade);
  _sizeLabel.alignment = NSTextAlignmentRight;
  _size = [NSSlider sliderWithValue:100 minValue:50 maxValue:180 target:target action:resize];
  _size.frame = NSMakeRect(77, 5, 143, 23);
  _size.continuous = YES;
  [_size setAccessibilityLabel:@"桌宠大小"];
  [_settingsView addSubview:_size];
  textLine(_settingsView, NSMakeRect(9, 40, 62, 19), 12, NSFontWeightSemibold, ink).stringValue = @"自动巡游";
  textLine(_settingsView, NSMakeRect(157, 40, 45, 19), 12, NSFontWeightSemibold, ink).stringValue = @"音效";
  _roam = toggle(_settingsView, NSMakeRect(80, 39, 47, 22), @"自动巡游", target, roam);
  _sound = toggle(_settingsView, NSMakeRect(223, 39, 44, 22), @"音效", target, sound);
  [self setTaskDetailsExpanded:NO];

  _beijing = [[NSDateFormatter alloc] init];
  _beijing.locale = [NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
  _beijing.timeZone = [NSTimeZone timeZoneWithName:@"Asia/Shanghai"];
  _beijing.dateFormat = @"MM/dd HH:mm";
  return self;
}

- (BOOL)canBecomeKeyWindow { return YES; }

- (CGFloat)bodyHeight { return paw::bubbleHeight + (_taskDetailsExpanded ? paw::bubbleDetailsHeight : 0); }
- (BOOL)taskDetailsExpanded { return _taskDetailsExpanded; }

- (void)setTaskDetailsExpanded:(BOOL)expanded {
  _taskDetailsExpanded = expanded;
  _taskBreakdown.hidden = _taskCache.hidden = _taskNote.hidden = !expanded;
  const CGFloat extra = expanded ? paw::bubbleDetailsHeight : 0;
  [_settingsView setFrameOrigin:NSMakePoint(12, 252 + extra)];
  _disclosure.title = expanded ? @"⌄" : @"›";
  _disclosure.image = symbol(expanded ? @"chevron.down" : @"chevron.right", 12);
  NSString* label = expanded ? @"收起任务明细" : @"展开任务明细";
  [_disclosure setAccessibilityLabel:label];
  _disclosure.toolTip = label;
}

- (void)showActions:(NSButton*)sender {
  [_actions popUpMenuPositioningItem:nil atLocation:NSMakePoint(0, -2) inView:sender];
}

- (void)setTailOnLeft:(BOOL)left offset:(CGFloat)offset {
  _bubble.tailOnLeft = left;
  _bubble.tailY = offset;
  _bubble.needsDisplay = YES;
  [self invalidateShadow];
}

- (void)cancelOperation:(id)sender {
  (void)sender;
  [self orderOut:nil];
}

- (void)setActivityText:(NSString*)text { _activity.stringValue = text; }

- (void)showBalance:(const paw::Balance&)balance {
  _refresh.enabled = balance.status != "loading";
  const bool ready = balance.status == "ready" && !balance.wallets.empty();
  const bool dual = ready && balance.wallets.size() == 2;
  _amount[0].frame = NSMakeRect(15, 76, dual ? 132 : 271, 29);
  _amount[0].font = [NSFont systemFontOfSize:dual ? 16 : ready ? 24 : 12 weight:NSFontWeightSemibold];
  _walletBonus[0].frame = NSMakeRect(15, dual ? 102 : 110, dual ? 132 : 271, 15);
  _balanceNotice.stringValue = ready && balance.available == "0" ? @"余额不足" : @"";
  for (int i = 0; i < 2; ++i) {
    _amount[i].stringValue = _walletBonus[i].stringValue = _walletCash[i].stringValue = @"";
    _amount[i].toolTip = _walletBonus[i].toolTip = _walletCash[i].toolTip = nil;
  }
  if (!ready) {
    _amount[0].stringValue = pawText(paw::balanceMessage(balance.status));
    _amount[0].toolTip = _amount[0].stringValue;
    return;
  }
  for (size_t i = 0; i < balance.wallets.size(); ++i) {
    const auto& wallet = balance.wallets[i];
    _amount[i].toolTip = _amount[i].stringValue = pawText(paw::money(wallet.currency, wallet.total));
    NSString* bonus = [@"赠送 " stringByAppendingString:pawText(paw::money(wallet.currency, wallet.bonus))];
    NSString* cash = [@"充值 " stringByAppendingString:pawText(paw::money(wallet.currency, wallet.cash))];
    _walletBonus[i].toolTip = _walletBonus[i].stringValue = dual ? bonus : [NSString stringWithFormat:@"%@  ·  %@", bonus, cash];
    if (dual) _walletCash[i].toolTip = _walletCash[i].stringValue = cash;
  }
}

- (void)showTask:(const std::optional<paw::Task>&)task balance:(const paw::Balance&)balance {
  _disclosure.hidden = !task;
  _taskUsage.frame = NSMakeRect(15, 227, task ? 147 : 271, 20);
  _taskUsage.toolTip = _taskUsage.stringValue = task ? pawText(paw::tokenText(*task)) : @"暂无已结束的任务";
  _taskPrice.toolTip = _taskPrice.stringValue = task ? pawText(paw::taskPrice(*task, balance)) : @"";
  _taskBreakdown.stringValue = task ? [NSString stringWithFormat:@"输入 %@  ·  输出 %@",
    pawText(paw::grouped(task->input)), pawText(paw::grouped(task->output))] : @"";
  _taskCache.stringValue = task ? [@"缓存命中 " stringByAppendingString:pawText(paw::grouped(task->cached))] : @"";
  _taskNote.stringValue = task ? pawText(paw::taskTitle(*task) + L"  ·  " + paw::taskCostNote(*task)) : @"";
}

- (void)showSize:(NSInteger)percent sound:(BOOL)sound roam:(BOOL)roam {
  _size.integerValue = percent;
  _sizeLabel.stringValue = [NSString stringWithFormat:@"%ld%%", long(percent)];
  _sound.state = sound ? NSControlStateValueOn : NSControlStateValueOff;
  _roam.state = roam ? NSControlStateValueOn : NSControlStateValueOff;
}

- (void)refreshClock {
  const int64_t now = int64_t(NSDate.date.timeIntervalSince1970 * 1000);
  const auto window = paw::tariffWindowAt(paw::tariffCalendar, now);
  const bool peak = window.current == paw::Period::peak;
  _period.hidden = _periodBadge.hidden = window.current == paw::Period::unknown;
  _period.stringValue = peak ? @"高峰时段" : @"空闲时段";
  _period.textColor = _remaining.textColor = panelColor(peak ? paw::bubblePeak : paw::bubbleOffPeak);
  _periodBadge.fillColor = panelColor(peak ? paw::bubblePeakFill : paw::bubbleOffPeakFill);
  if (!window.next) {
    _remaining.font = [NSFont systemFontOfSize:13 weight:NSFontWeightMedium];
    _remaining.textColor = panelColor(0x655e55);
    _remaining.stringValue = @"后续计费日历待更新";
    _deadline.stringValue = @"";
    return;
  }
  const auto seconds = paw::secondsUntil(now, window.next->at);
  _remaining.font = [NSFont monospacedDigitSystemFontOfSize:seconds >= 86400 ? 16 : 22 weight:NSFontWeightSemibold];
  _remaining.stringValue = duration(seconds);
  NSDate* deadline = [NSDate dateWithTimeIntervalSince1970:window.next->at / 1000.0];
  NSString* next = window.next->period == paw::Period::peak ? @"下次高峰" : @"下次空闲";
  _deadline.stringValue = [NSString stringWithFormat:@"%@  %@", next, [_beijing stringFromDate:deadline]];
}
@end
