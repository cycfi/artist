/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#import <Cocoa/Cocoa.h>
#include <dlfcn.h>
#include <string>
#include <stdexcept>
#include <chrono>
#include "../../app.hpp"
#include <artist/resources.hpp>

#include <OpenGL/gl.h>

using namespace cycfi::artist;

// rendering elapsed time
float elapsed_ = 0;

///////////////////////////////////////////////////////////////////////////////
// Helper utils
namespace
{
   CFBundleRef get_bundle_from_executable(const char* filepath)
   {
      NSString* exec_str = [NSString stringWithCString:filepath encoding : NSUTF8StringEncoding];
      NSString* mac_os_str = [exec_str stringByDeletingLastPathComponent];
      NSString* contents_str = [mac_os_str stringByDeletingLastPathComponent];
      NSString* bundleStr = [contents_str stringByDeletingLastPathComponent];
      return CFBundleCreate(0, (CFURLRef)[NSURL fileURLWithPath:bundleStr isDirectory : YES]);
   }

   CFBundleRef get_current_bundle()
   {
      Dl_info info;
      if (dladdr((const void*)get_current_bundle, &info) && info.dli_fname)
         return get_bundle_from_executable(info.dli_fname);
      return 0;
   }

   void get_resource_path(char resource_path[])
   {
      CFBundleRef main_bundle = get_current_bundle();
      CFURLRef resources_url = CFBundleCopyResourcesDirectoryURL(main_bundle);
      CFURLGetFileSystemRepresentation(resources_url, TRUE, (UInt8*) resource_path, PATH_MAX);
      CFRelease(resources_url);
   }
}

namespace cycfi::artist
{
   void init_paths()
   {
      // Before anything else, set the working directory so we can access
      // our resources
      char resource_path[PATH_MAX];
      get_resource_path(resource_path);
      add_search_path(resource_path);
   }

   // This is declared in font.hpp
   fs::path get_user_fonts_directory()
   {
      char resource_path[PATH_MAX];
      get_resource_path(resource_path);
      return fs::path(resource_path);
   }
}

//=======================================================================

@class OpenGLLayer;
using offscreen_type = std::shared_ptr<image>;

@interface CocoaView : NSView
{
   NSTimer*       _task;
   bool           _first;
   offscreen_type _offscreen;
}

-(void) start;
-(void) start_animation;

@end

//=======================================================================

@implementation CocoaView

- (void) dealloc
{
   _task = nil;
}

- (void) start
{
   _first = true;
   _task = nullptr;
}

- (void) drawRect : (NSRect) dirty
{
   auto start = std::chrono::high_resolution_clock::now();
   auto ctx = NSGraphicsContext.currentContext.CGContext;

   if (_first && _task)
   {
      _first = false;
      _offscreen = std::make_shared<image>(
         extent{ float(self.bounds.size.width), float(self.bounds.size.height) }
      );
   }

   if (_task)
   {
      auto cnv = canvas{ (canvas_impl_ptr) ctx };
      {
         // Do offscreen rendering
         offscreen_image ctx{ *_offscreen };
         canvas offscreen_cnv{ ctx.context() };
         draw(offscreen_cnv);
      }
      cnv.draw(*_offscreen);
   }
   else
   {
      auto cnv = canvas{ (canvas_impl_ptr) ctx };
      draw(cnv);
   }

   auto stop = std::chrono::high_resolution_clock::now();
   elapsed_ = std::chrono::duration<double>{ stop - start }.count();
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

@end

//=======================================================================
class window
{
public:

   window(extent window_size, color bkd)
   {
      _window =
         [[NSWindow alloc]
            initWithContentRect : NSMakeRect(0, 0, window_size.x, window_size.y)
                      styleMask : NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                        backing : NSBackingStoreBuffered
                          defer : NO
         ];

      auto color =
           [NSColor colorWithRed : bkd.red
                           green : bkd.green
                            blue : bkd.blue
                           alpha : bkd.alpha
           ];

      _content = [[CocoaView alloc] init];
      [_content start];
      [_window setContentView : _content];
      [_window cascadeTopLeftFromPoint : NSMakePoint(20, 20)];
      [_window makeKeyAndOrderFront : nil];
      [_window setAppearance : [NSAppearance appearanceNamed : NSAppearanceNameVibrantDark]];
      [_window setBackgroundColor : color];
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

int run_app(
   int argc
 , char const* argv[]
 , extent window_size
 , color bkd
 , bool animate
)
{
   app _app;
   window _win(window_size, bkd);
   if (animate)
      _win.start_animation();
   return _app.run();
}

void print_elapsed(canvas& cnv, point br)
{
   static font open_sans = font_descr{ "Open Sans", 12 };
   static int i = 0;
   static float t_elapsed = 0;
   static float c_elapsed = 0;

   if (++i == 30)
   {
      i = 0;
      c_elapsed = t_elapsed / 30;
      t_elapsed = 0;
   }
   else
   {
      t_elapsed += elapsed_;
   }

   if (c_elapsed)
   {
      cnv.fill_style(rgba(220, 220, 220, 200));
      cnv.font(open_sans);
      cnv.text_align(cnv.right | cnv.bottom);
      cnv.fill_text(std::to_string(1 / c_elapsed) + " fps", { br.x, br.y });
   }
}
