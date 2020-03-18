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
}

-(void) start;

@end

@implementation CocoaView

- (void) start
{
   _task =
      [NSTimer scheduledTimerWithTimeInterval : 0.016 // 60Hz
           target : self
         selector : @selector(on_tick:)
         userInfo : nil
          repeats : YES
      ];
}

- (void) drawRect : (NSRect) dirty
{
   [super drawRect : dirty];

   auto ctx = NSGraphicsContext.currentContext.CGContext;
   auto cnv = canvas{ (host_context_ptr) ctx };
   draw(cnv);
}

-(BOOL) isFlipped
{
   return YES;
}

- (void) on_tick : (id) sender
{
   [self setNeedsDisplay : YES];
}

- (BOOL) isOpaque { return YES; }

@end

//=======================================================================
class window
{
public:

   window()
   {
      _window =
         [[NSWindow alloc]
            initWithContentRect : NSMakeRect(0, 0, 640, 480)
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

window* _win_ptr = nullptr;

int run_app(int argc, const char* argv[])
{
   app _app;
   window _win;
   _win_ptr = &_win;
   return _app.run();
}

void stop_app()
{
   [NSApp terminate : nil];
}
