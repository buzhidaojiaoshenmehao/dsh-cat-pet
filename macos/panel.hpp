#pragma once

#import <AppKit/AppKit.h>

#include "protocol.hpp"

inline NSString* pawText(const std::wstring& text) {
  return [[NSString alloc] initWithBytes:text.data() length:text.size() * sizeof(wchar_t)
    encoding:NSUTF32LittleEndianStringEncoding] ?: @"";
}

@interface PawPanel : NSPanel
@property(nonatomic) BOOL taskDetailsExpanded;
@property(nonatomic, readonly) CGFloat bodyHeight;
- (instancetype)initWithTarget:(id)target refresh:(SEL)refresh resize:(SEL)resize
  sound:(SEL)sound roam:(SEL)roam details:(SEL)details menu:(NSMenu*)menu portrait:(NSImage*)portrait;
- (void)setActivityText:(NSString*)text;
- (void)showBalance:(const paw::Balance&)balance;
- (void)showTask:(const std::optional<paw::Task>&)task balance:(const paw::Balance&)balance;
- (void)showSize:(NSInteger)percent sound:(BOOL)sound roam:(BOOL)roam;
- (void)setTailOnLeft:(BOOL)left offset:(CGFloat)offset;
- (void)refreshClock;
@end
