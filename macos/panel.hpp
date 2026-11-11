#pragma once

#import <AppKit/AppKit.h>

#include "protocol.hpp"

inline NSString* pawText(const std::wstring& text) {
  return [[NSString alloc] initWithBytes:text.data() length:text.size() * sizeof(wchar_t)
    encoding:NSUTF32LittleEndianStringEncoding] ?: @"";
}

@interface PawPanel : NSPanel
- (instancetype)initWithTarget:(id)target refresh:(SEL)refresh resize:(SEL)resize sound:(SEL)sound;
- (void)setActivityText:(NSString*)text;
- (void)showBalance:(const paw::Balance&)balance;
- (void)showTask:(const std::optional<paw::Task>&)task balance:(const paw::Balance&)balance;
- (void)showSize:(NSInteger)percent sound:(BOOL)enabled;
- (void)refreshClock;
@end
