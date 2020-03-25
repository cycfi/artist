/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#import <Cocoa/Cocoa.h>
#include <string>
#include <stdexcept>
#include "../../app.hpp"

#if defined(ARTIST_SKIA)
#include "GrContext.h"
#include "gl/GrGLInterface.h"
#include "SkImage.h"
#include "SkSurface.h"


#include "SkBitmap.h"
#include "SkData.h"
#include "SkImage.h"
#include "SkPicture.h"
#include "SkSurface.h"
#include "SkCanvas.h"
#include "SkPath.h"
#include "GrBackendSurface.h"

#endif

#include <OpenGL/gl.h>

using namespace cycfi::artist;

//=======================================================================

@class OpenGLLayer;
using offscreen_type = std::shared_ptr<picture>;

@interface CocoaView : NSView
{
#if defined(ARTIST_SKIA)
   OpenGLLayer*   _layer;
#endif

   NSTimer*       _task;

#if defined(ARTIST_QUARTZ_2D)

   bool           _first;
   offscreen_type _offscreen;
#endif
}

-(void) start;
-(void) start_animation;

@end

#if defined(ARTIST_SKIA)

@interface OpenGLLayer : NSOpenGLLayer
{
   bool                 _refresh;
   bool                 _first;
   CocoaView*           _view;
}

- (id) initWithIGraphicsView : (CocoaView*) view;
- (void) refresh;

@end

#endif // ARTIST_SKIA

//=======================================================================

#if defined(ARTIST_SKIA)

@implementation OpenGLLayer

- (id) initWithIGraphicsView: (CocoaView*) view;
{
   _refresh = true;
   _first = true;
   _view = view;
   self = [super init];
   if (self != nil)
   {
      // Layer should render when size changes.
      self.needsDisplayOnBoundsChange = YES;

      // The layer should continuously call canDrawInOpenGLContext
      self.asynchronous = YES;
   }

   return self;
}

- (void) refresh
{
   _refresh = true;
}

- (NSOpenGLPixelFormat*) openGLPixelFormatForDisplayMas : (uint32_t) mask
{
   NSOpenGLPixelFormatAttribute attr[] = {
      NSOpenGLPFAOpenGLProfile,
      NSOpenGLProfileVersion3_2Core,
      NSOpenGLPFANoRecovery,
      NSOpenGLPFAAccelerated,
      NSOpenGLPFADoubleBuffer,
      NSOpenGLPFAColorSize, 24,
      0
   };
   return [[NSOpenGLPixelFormat alloc] initWithAttributes:attr];
}

- (NSOpenGLContext*) openGLContextForPixelFormat : (NSOpenGLPixelFormat*) pixelFormat
{
   return [super openGLContextForPixelFormat : pixelFormat];
}

- (BOOL) canDrawInOpenGLContext : (NSOpenGLContext*) context
                    pixelFormat : (NSOpenGLPixelFormat*) pixelFormat
                   forLayerTime : (CFTimeInterval) timeInterval
                    displayTime : (const CVTimeStamp*) timeStamp
{
   return _refresh;
}

- (void) drawInOpenGLContext : (NSOpenGLContext*) context
                 pixelFormat : (NSOpenGLPixelFormat*) pixelFormat
                forLayerTime : (CFTimeInterval) timeInterval
                 displayTime : (const CVTimeStamp*) timeStamp
{
   _refresh = false;

   [context makeCurrentContext];

   CGLLockContext(context.CGLContextObj);

   auto interface = GrGLMakeNativeInterface();
   sk_sp<GrContext> ctx = GrContext::MakeGL(interface);

   GrGLint buffer;
   glGetIntegerv(GL_FRAMEBUFFER_BINDING, &buffer);
   GrGLFramebufferInfo info;
   info.fFBOID = (GrGLuint) buffer;
   SkColorType colorType = kRGBA_8888_SkColorType;

   auto bounds = [_view bounds];
   auto scale = self.contentsScale;
   auto size = point{ float(bounds.size.width*scale), float(bounds.size.height*scale) };

   info.fFormat = GL_RGBA8;
   GrBackendRenderTarget target(size.x, size.y, 0, 8, info);

   sk_sp<SkSurface> surface(
      SkSurface::MakeFromBackendRenderTarget(ctx.get(), target,
      kBottomLeft_GrSurfaceOrigin, colorType, nullptr, nullptr));

   if (!surface)
      throw std::runtime_error("Error: SkSurface::MakeRenderTarget returned null");

   SkCanvas* gpu_canvas = surface->getCanvas();
   auto cnv = canvas{ gpu_canvas };
   cnv.pre_scale({ float(scale), float(scale) });
   draw(cnv);

   [context flushBuffer];
   CGLUnlockContext(context.CGLContextObj);
}

@end

#endif // ARTIST_SKIA

//=======================================================================

@implementation CocoaView

- (void) dealloc
{
   _task = nil;
}

- (void) start
{
#if defined(ARTIST_SKIA)
   // Enable retina-support
   self.wantsBestResolutionOpenGLSurface = YES;

   // Enable layer-backed drawing of view
   [self setWantsLayer : YES];

   self.layer.opaque = YES;
#endif

#if defined(ARTIST_QUARTZ_2D)
   _first = true;
   _task = nullptr;
#endif
}

- (void) drawRect : (NSRect) dirty
{
#if defined(ARTIST_QUARTZ_2D)
   auto ctx = NSGraphicsContext.currentContext.CGContext;

   if (_first && _task)
   {
      _first = false;
      _offscreen = std::make_shared<picture>(
         extent{ float(self.bounds.size.width), float(self.bounds.size.height) }
      );
   }

   if (_task)
   {
      auto cnv = canvas{ (host_context_ptr) ctx };
      {
         // Do offscreen rendering
         picture_context ctx{ *_offscreen };
         canvas offscreen_cnv{ ctx.context() };
         draw(offscreen_cnv);
      }
      cnv.draw(*_offscreen);
   }
   else
   {
      auto cnv = canvas{ (host_context_ptr) ctx };
      draw(cnv);
   }
#endif
}

#if defined(ARTIST_SKIA)

- (CALayer*) makeBackingLayer
{
   _layer = [[OpenGLLayer alloc] initWithIGraphicsView : self];
   return _layer;
}

- (void) viewDidChangeBackingProperties
{
   [super viewDidChangeBackingProperties];
   self.layer.contentsScale = self.window.backingScaleFactor;
}

#endif // ARTIST_SKIA

-(BOOL) isFlipped
{
   return YES;
}

- (void) on_tick : (id) sender
{
#if defined(ARTIST_QUARTZ_2D)
   [self setNeedsDisplay : YES];
#endif

#if defined(ARTIST_SKIA)
   [_layer refresh];
#endif
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
   if (animate)
      _win.start_animation();
   return _app.run();
}

void stop_app()
{
   [NSApp terminate : nil];
}
