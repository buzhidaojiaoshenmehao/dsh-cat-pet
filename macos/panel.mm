#import "panel.hpp"

#include "tariff.hpp"
#include "pricing-calendar.hpp"

@interface FlippedContent : NSView
@end

@implementation FlippedContent
- (BOOL)isFlipped { return YES; }
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

void card(NSView* view, CGFloat y, CGFloat height, NSString* title) {
  NSBox* box = [[NSBox alloc] initWithFrame:NSMakeRect(16, y, 356, height)];
  box.boxType = NSBoxCustom;
  box.titlePosition = NSNoTitle;
  box.fillColor = NSColor.controlBackgroundColor;
  box.borderColor = NSColor.separatorColor;
  box.cornerRadius = 12;
  [view addSubview:box];
  textLine(view, NSMakeRect(29, y + 11, 220, 20), 13, NSFontWeightSemibold, NSColor.labelColor).stringValue = title;
}

NSString* duration(int64_t seconds) {
  const auto days = seconds / 86400;
  NSString* clock = [NSString stringWithFormat:@"%02d:%02d:%02d", int(seconds / 3600 % 24),
    int(seconds / 60 % 60), int(seconds % 60)];
  return days ? [NSString stringWithFormat:@"%lld 天 %@", static_cast<long long>(days), clock] : clock;
}

}

@implementation PawPanel {
  NSTextField* _activity;
  NSTextField* _amount;
  NSTextField* _walletDetails[2];
  NSButton* _refresh;
  NSTextField* _period;
  NSTextField* _remaining;
  NSTextField* _deadline;
  NSTextField* _taskStatus;
  NSTextField* _taskUsage;
  NSTextField* _taskPrice;
  NSTextField* _taskBreakdown;
  NSTextField* _taskNote;
  NSTextField* _sizeLabel;
  NSSlider* _size;
  NSButton* _sound;
  NSDateFormatter* _beijing;
}

- (instancetype)initWithTarget:(id)target refresh:(SEL)refresh resize:(SEL)resize sound:(SEL)sound {
  self = [super initWithContentRect:NSMakeRect(0, 0, 388, 444)
    styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskNonactivatingPanel
    backing:NSBackingStoreBuffered defer:NO];
  if (!self) return nil;
  self.title = @"小爪同伴";
  self.releasedWhenClosed = NO;
  self.floatingPanel = YES;
  self.hidesOnDeactivate = NO;
  self.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorFullScreenAuxiliary;
  FlippedContent* view = [[FlippedContent alloc] initWithFrame:NSMakeRect(0, 0, 388, 444)];
  self.contentView = view;
  NSColor* ink = NSColor.labelColor;
  NSColor* quiet = NSColor.secondaryLabelColor;
  _activity = textLine(view, NSMakeRect(18, 13, 352, 19), 12, NSFontWeightMedium, quiet);

  card(view, 39, 105, @"DeepSeek 余额");
  _refresh = [NSButton buttonWithTitle:@"刷新" target:target action:refresh];
  _refresh.frame = NSMakeRect(295, 45, 64, 29);
  _refresh.bezelStyle = NSBezelStyleRounded;
  [_refresh setAccessibilityLabel:@"刷新余额"];
  [view addSubview:_refresh];
  _amount = textLine(view, NSMakeRect(29, 73, 329, 27), 19, NSFontWeightSemibold, ink);
  _walletDetails[0] = textLine(view, NSMakeRect(29, 100, 329, 18), 11, NSFontWeightRegular, quiet);
  _walletDetails[1] = textLine(view, NSMakeRect(29, 117, 329, 18), 11, NSFontWeightRegular, quiet);

  card(view, 154, 79, @"峰谷倒计时");
  _period = textLine(view, NSMakeRect(226, 165, 132, 18), 11, NSFontWeightSemibold, quiet);
  _period.alignment = NSTextAlignmentRight;
  _remaining = textLine(view, NSMakeRect(29, 188, 160, 28), 20, NSFontWeightSemibold, ink);
  _remaining.font = [NSFont monospacedDigitSystemFontOfSize:20 weight:NSFontWeightSemibold];
  _deadline = textLine(view, NSMakeRect(185, 192, 173, 21), 11, NSFontWeightRegular, quiet);
  _deadline.alignment = NSTextAlignmentRight;

  card(view, 243, 113, @"最近任务");
  _taskStatus = textLine(view, NSMakeRect(238, 254, 120, 19), 11, NSFontWeightMedium, quiet);
  _taskStatus.alignment = NSTextAlignmentRight;
  _taskUsage = textLine(view, NSMakeRect(29, 278, 214, 24), 17, NSFontWeightSemibold, ink);
  _taskPrice = textLine(view, NSMakeRect(235, 278, 123, 24), 12, NSFontWeightSemibold, ink);
  _taskPrice.alignment = NSTextAlignmentRight;
  _taskBreakdown = textLine(view, NSMakeRect(29, 307, 329, 18), 11, NSFontWeightRegular, quiet);
  _taskNote = textLine(view, NSMakeRect(29, 325, 329, 18), 11, NSFontWeightRegular, quiet);

  card(view, 366, 62, @"桌宠大小");
  _sizeLabel = textLine(view, NSMakeRect(161, 377, 57, 20), 12, NSFontWeightMedium, quiet);
  _sizeLabel.alignment = NSTextAlignmentRight;
  _size = [NSSlider sliderWithValue:100 minValue:50 maxValue:180 target:target action:resize];
  _size.frame = NSMakeRect(29, 397, 189, 23);
  _size.continuous = YES;
  [_size setAccessibilityLabel:@"桌宠大小"];
  [view addSubview:_size];
  _sound = [NSButton checkboxWithTitle:@"音效" target:target action:sound];
  _sound.frame = NSMakeRect(251, 386, 107, 24);
  [view addSubview:_sound];

  _beijing = [[NSDateFormatter alloc] init];
  _beijing.locale = [NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
  _beijing.timeZone = [NSTimeZone timeZoneWithName:@"Asia/Shanghai"];
  _beijing.dateFormat = @"MM/dd HH:mm";
  return self;
}

- (void)cancelOperation:(id)sender {
  (void)sender;
  [self orderOut:nil];
}

- (void)setActivityText:(NSString*)text { _activity.stringValue = text; }

- (void)showBalance:(const paw::Balance&)balance {
  _refresh.enabled = balance.status != "loading";
  const bool ready = balance.status == "ready" && !balance.wallets.empty();
  _amount.font = [NSFont systemFontOfSize:ready ? 19 : 13 weight:NSFontWeightSemibold];
  _walletDetails[0].stringValue = @"";
  _walletDetails[1].stringValue = @"";
  if (!ready) {
    _amount.stringValue = pawText(paw::balanceMessage(balance.status));
    return;
  }
  NSMutableArray<NSString*>* totals = [NSMutableArray arrayWithCapacity:balance.wallets.size()];
  for (size_t i = 0; i < balance.wallets.size(); ++i) {
    const auto& wallet = balance.wallets[i];
    [totals addObject:pawText(paw::money(wallet.currency, wallet.total))];
    _walletDetails[i].stringValue = [NSString stringWithFormat:@"赠送 %@    充值 %@",
      pawText(paw::money(wallet.currency, wallet.bonus)), pawText(paw::money(wallet.currency, wallet.cash))];
  }
  if (balance.available == "0") [totals addObject:@"余额不足"];
  _amount.stringValue = [totals componentsJoinedByString:@"  ·  "];
}

- (void)showTask:(const std::optional<paw::Task>&)task balance:(const paw::Balance&)balance {
  _taskUsage.font = [NSFont systemFontOfSize:task ? 17 : 12 weight:NSFontWeightSemibold];
  _taskStatus.stringValue = task ? pawText(paw::taskTitle(*task)) : @"";
  _taskUsage.stringValue = task ? pawText(paw::tokenText(*task)) : @"任务结束后显示用量和费用";
  _taskPrice.stringValue = task ? pawText(paw::taskPrice(*task, balance)) : @"";
  _taskBreakdown.stringValue = task ? [NSString stringWithFormat:@"输入 %@  ·  输出 %@  ·  缓存命中 %@",
    pawText(paw::grouped(task->input)), pawText(paw::grouped(task->output)), pawText(paw::grouped(task->cached))] : @"";
  _taskNote.stringValue = !task ? @"" : task->incomplete ? @"部分用量未返回" : task->coverage == "full"
    ? @"模型费用按官方峰谷价估算" : task->coverage == "partial" ? @"仅显示可计价的模型费用" : @"当前模型费用无可用价格";
}

- (void)showSize:(NSInteger)percent sound:(BOOL)enabled {
  _size.integerValue = percent;
  _sizeLabel.stringValue = [NSString stringWithFormat:@"%ld%%", long(percent)];
  _sound.state = enabled ? NSControlStateValueOn : NSControlStateValueOff;
}

- (void)refreshClock {
  const int64_t now = int64_t(NSDate.date.timeIntervalSince1970 * 1000);
  const auto window = paw::tariffWindowAt(paw::tariffCalendar, now);
  _period.stringValue = window.current == paw::Period::unknown ? @"北京时间"
    : window.current == paw::Period::peak ? @"高峰时段 · 北京时间" : @"空闲时段 · 北京时间";
  if (!window.next) {
    _remaining.font = [NSFont systemFontOfSize:13 weight:NSFontWeightMedium];
    _remaining.stringValue = @"后续计费日历待更新";
    _deadline.stringValue = @"";
    return;
  }
  _remaining.font = [NSFont monospacedDigitSystemFontOfSize:20 weight:NSFontWeightSemibold];
  _remaining.stringValue = duration(paw::secondsUntil(now, window.next->at));
  NSDate* deadline = [NSDate dateWithTimeIntervalSince1970:window.next->at / 1000.0];
  NSString* next = window.next->period == paw::Period::peak ? @"下次高峰" : @"下次空闲";
  _deadline.stringValue = [NSString stringWithFormat:@"%@  %@", next, [_beijing stringFromDate:deadline]];
}
@end
