/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#include <dlfcn.h>
#include <string>
#include <stdexcept>
#include <chrono>
#include <cmath>
#include "../../app.hpp"
#include <artist/resources.hpp>

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

@interface CocoaView : NSView
{
   NSTimer*            _task;
   CGContextRef        _perf_ctx;
}

-(void) start;
-(void) start_animation;
-(void) render;

@end

//=======================================================================

@implementation CocoaView

- (void) dealloc
{
   _task = nil;
   if (_perf_ctx)
      CGContextRelease(_perf_ctx);
   _perf_ctx = nullptr;
}

- (void) start
{
   self.wantsLayer = YES;
}

- (void) drawRect : (NSRect) dirty
{
   // When measuring, frames are drawn only by render, into the host's own 1x
   // bitmap. Drawing here too would size lazily made layers for this 2x
   // context.
   if (perf_enabled())
      return;

   auto cg_ctx = NSGraphicsContext.currentContext.CGContext;

   // Clip to the real view bounds so the canvas clip_extent() reports the
   // true logical window size. AppKit hands drawRect: a context whose clip
   // extends over the title-bar strip (e.g. 640x512 for a 640x480 view),
   // which would push reflowed content (FPS readout, bounce bounds) off
   // screen. Reflow examples depend on clip_extent() == view size.
   CGContextClipToRect(cg_ctx, CGRectMake(0, 0, self.bounds.size.width, self.bounds.size.height));

   auto cnv = canvas{(canvas_impl*) cg_ctx};
   draw(cnv);
}

// One frame. Normally AppKit draws the view and presents it on its own
// schedule. When measuring (ARTIST_PERF), that would leave most of the
// rasterizing outside the frame time, so the view's own drawing is
// rasterized into a bitmap now and the bitmap is presented; the frame time
// covers finished pixels and the present. The bitmap is 1x, so the pixel
// count matches a scale-1 display on the other machines.
- (void) render
{
   auto start = std::chrono::steady_clock::now();
   if (perf_enabled())
   {
      // A bitmap context of the host's own, not cacheDisplayInRect: AppKit's
      // context reports the window's backing scale even when its bitmap is
      // 1x, so the canvas and its layers would size themselves for 2x.
      auto const bounds = self.bounds;
      size_t const w = size_t(bounds.size.width);
      size_t const h = size_t(bounds.size.height);
      if (!_perf_ctx
         || CGBitmapContextGetWidth(_perf_ctx) != w
         || CGBitmapContextGetHeight(_perf_ctx) != h)
      {
         if (_perf_ctx)
            CGContextRelease(_perf_ctx);
         auto space = CGColorSpaceCreateDeviceRGB();
         _perf_ctx = CGBitmapContextCreate(
            nullptr, w, h, 8, 0, space,
            CGBitmapInfo(uint32_t(kCGBitmapByteOrder32Little)
               | uint32_t(kCGImageAlphaPremultipliedFirst)));
         CGColorSpaceRelease(space);
      }

      CGContextSaveGState(_perf_ctx);
      CGContextClearRect(_perf_ctx, CGRectMake(0, 0, w, h));
      CGContextTranslateCTM(_perf_ctx, 0, h);
      CGContextScaleCTM(_perf_ctx, 1, -1);
      {
         auto cnv = canvas{(canvas_impl*) _perf_ctx};
         draw(cnv);
      }
      CGContextRestoreGState(_perf_ctx);

      auto image = CGBitmapContextCreateImage(_perf_ctx);
      [CATransaction begin];
      [CATransaction setDisableActions : YES];
      self.layer.contents = (__bridge id) image;
      [CATransaction commit];
      CGImageRelease(image);
   }
   else
   {
      [self display];
   }
   [CATransaction flush];
   auto stop = std::chrono::steady_clock::now();

   elapsed_ = std::chrono::duration<double>{stop - start}.count();
   if (perf_enabled())
      perf_record(elapsed_, int(self.bounds.size.width), int(self.bounds.size.height));
}

-(BOOL) isFlipped
{
   return YES;
}

// Redraw the whole view as the window resizes so example content reflows live
// (matches the other hosts: resizable window + redraw at the new size).
- (void) setFrameSize : (NSSize) newSize
{
   [super setFrameSize : newSize];
   [self setNeedsDisplay : YES];
}

- (void) on_tick : (id) sender
{
   [self render];
}

-(void) start_animation
{
   // When measuring (ARTIST_PERF), redraw as fast as the run loop allows.
   _task =
      [NSTimer scheduledTimerWithTimeInterval : perf_enabled()? 0.0 : 1.0/60
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
                                | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable
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
      [_window setContentView : _content];
      [_window cascadeTopLeftFromPoint : NSMakePoint(20, 20)];
      [_window makeKeyAndOrderFront : nil];
      [_window setAppearance : [NSAppearance appearanceNamed : NSAppearanceNameVibrantDark]];
      [_window setBackgroundColor : color];
      [_content start];
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
   if (animate || perf_enabled())
      _win.start_animation();
   return _app.run();
}
