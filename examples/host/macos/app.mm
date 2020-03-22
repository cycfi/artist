/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#import <Cocoa/Cocoa.h>
#include <string>
#include "../../app.hpp"

using namespace cycfi::artist;

//=======================================================================

@interface CocoaView : NSView
{
   NSTimer*    _task;
   CGLayerRef  _layer;
   bool        _first;
}

-(void) start;
-(void) start_animation;

@end

@implementation CocoaView

- (void) dealloc
{
   _task = nil;
   CGLayerRelease(_layer);
}

- (void) start
{
   _first = true;
}

- (void) drawRect : (NSRect) dirty
{
   auto ctx = NSGraphicsContext.currentContext.CGContext;

   if (_first)
   {
      _first = false;
      _layer = CGLayerCreateWithContext(ctx, self.bounds.size, nullptr);
   }

   auto offline_ctx = CGLayerGetContext(_layer);
   auto cnv = canvas{ (host_context_ptr) offline_ctx };
   draw(cnv);

   CGContextDrawLayerAtPoint(ctx, CGPointZero, _layer);
}

-(BOOL) isFlipped
{
   return YES;
}

- (void) on_tick : (id) sender
{
   [self setNeedsDisplay : YES];
}

-(void) start_animation
{
   _task =
      [NSTimer scheduledTimerWithTimeInterval : 0.016 // 60Hz
           target : self
         selector : @selector(on_tick:)
         userInfo : nil
          repeats : YES
      ];
}

//- (BOOL) isOpaque { return YES; }

@end

//=======================================================================
class window
{
public:

   window(extent window_size)
   {
      _window =
         [[NSWindow alloc]
            initWithContentRect : NSMakeRect(0, 0, window_size.x, window_size.y)
            styleMask :
               NSWindowStyleMaskTitled |
               NSWindowStyleMaskClosable |
               NSWindowStyleMaskMiniaturizable |
               NSWindowStyleMaskResizable
            backing : NSBackingStoreBuffered
            defer : NO
         ];

      _content = [[CocoaView alloc] init];
      [_content start];
      [_window setContentView : _content];
      [_window cascadeTopLeftFromPoint : NSMakePoint(20, 20)];
      [_window makeKeyAndOrderFront : nil];
      [_window setAppearance : [NSAppearance appearanceNamed : NSAppearanceNameVibrantDark]];
   }

   void start_animation()
   {
      [_content start_animation];
   }

private:

   NSWindow*   _window;
   CocoaView*  _content;
};

//=======================================================================
class app
{
public:

   app()
   {
      [NSApplication sharedApplication];
      [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

      _menubar = [NSMenu new];
      id app_menu_item = [NSMenuItem new];
      [_menubar addItem : app_menu_item];
      [NSApp setMainMenu : _menubar];
      id app_menu = [NSMenu new];
      id quitTitle = @"Quit";
      id quitMenuItem = [[NSMenuItem alloc] initWithTitle : quitTitle
         action:@selector(terminate:) keyEquivalent:@"q"];
      [app_menu addItem:quitMenuItem];
      [app_menu_item setSubmenu : app_menu];
   }

   int run()
   {
      [NSApp activateIgnoringOtherApps:YES];
      [NSApp run];
      return 0;
   }

private:

   id _menubar;
};

int run_app(int argc, const char* argv[], extent window_size, bool animate)
{
   app _app;
   window _win(window_size);
   _win.start_animation();
   return _app.run();
}

void stop_app()
{
   [NSApp terminate : nil];
}
