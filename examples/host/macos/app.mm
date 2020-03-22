/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#import <Cocoa/Cocoa.h>
#include <string>
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

@interface CocoaView : NSView
{
   NSTimer*       _task;
}

-(void) start;
-(void) start_animation;

@end

#if defined(ARTIST_SKIA)

@interface OpenGLLayer : NSOpenGLLayer
{
   CocoaView* _view;
}

- (id) initWithIGraphicsView : (CocoaView*) view;

@end

#endif // ARTIST_SKIA

//=======================================================================

#if defined(ARTIST_SKIA)

@implementation OpenGLLayer

- (id) initWithIGraphicsView: (CocoaView*) view;
{
   _view = view;
   self = [super init];
   if ( self != nil )
   {
      // Layer should render when size changes.
      self.needsDisplayOnBoundsChange = YES;

      // The layer should continuously call canDrawInOpenGLContext
      self.asynchronous = YES;
   }

   return self;
}

- (NSOpenGLPixelFormat *)openGLPixelFormatForDisplayMask:(uint32_t)mask
{
   NSOpenGLPixelFormatAttribute attr[] = {
      NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
      NSOpenGLPFANoRecovery,
      NSOpenGLPFAAccelerated,
      NSOpenGLPFADoubleBuffer,
      NSOpenGLPFAColorSize, 24,
      0
   };
   return [[NSOpenGLPixelFormat alloc] initWithAttributes:attr];
}

- (NSOpenGLContext*)openGLContextForPixelFormat:(NSOpenGLPixelFormat *)pixelFormat
{
   return [super openGLContextForPixelFormat:pixelFormat];
}

- (BOOL)canDrawInOpenGLContext:(NSOpenGLContext *)context pixelFormat:(NSOpenGLPixelFormat *)pixelFormat forLayerTime:(CFTimeInterval)timeInterval displayTime:(const CVTimeStamp *)timeStamp
{
   return YES;
}

- (void)drawInOpenGLContext:(NSOpenGLContext *)context pixelFormat:(NSOpenGLPixelFormat *)pixelFormat forLayerTime:(CFTimeInterval)timeInterval displayTime:(const CVTimeStamp *)timeStamp
{
   [context makeCurrentContext];

   CGLLockContext(context.CGLContextObj);


   auto interface = GrGLMakeNativeInterface();
   sk_sp<GrContext> ctx = GrContext::MakeGL(interface);

   GrGLint buffer;
   glGetIntegerv(GL_FRAMEBUFFER_BINDING, &buffer);
   GrGLFramebufferInfo info;
   info.fFBOID = (GrGLuint) buffer;
   SkColorType colorType;

   auto scale = self.contentsScale;

   auto b = [_view bounds];
   info.fFormat = GL_RGBA8;
   colorType = kRGBA_8888_SkColorType;
   GrBackendRenderTarget target(b.size.width*scale, b.size.height*scale, 0, 8, info);

   sk_sp<SkSurface> surface(
      SkSurface::MakeFromBackendRenderTarget(ctx.get(), target,
      kBottomLeft_GrSurfaceOrigin, colorType, nullptr, nullptr));


   //SkImageInfo info = SkImageInfo:: MakeN32Premul(640, 480); // $$$ HARD CODE !!! $$$

   // sk_sp<SkSurface> gpuSurface(
   //    SkSurface::MakeRenderTarget(ctx.get(), SkBudgeted::kNo, info));

   if (!surface)
   {
      SkDebugf("SkSurface::MakeRenderTarget returned null\n");
      return;
   }

   SkCanvas* gpuCanvas = surface->getCanvas();


   // {
   //    const SkScalar scale = 256.0f;
   //    const SkScalar R = 0.45f * scale;
   //    const SkScalar TAU = 6.2831853f;
   //    SkPath path;
   //    path.moveTo(R, 0.0f);
   //    for (int i = 1; i < 7; ++i) {
   //       SkScalar theta = 3 * i * TAU / 7;
   //       path.lineTo(R * cos(theta), R * sin(theta));
   //    }
   //    path.close();
   //    SkPaint p;
   //    p.setAntiAlias(true);
   //    canvas->clear(SK_ColorWHITE);
   //    canvas->translate(0.5f * scale, 0.5f * scale);
   //    canvas->drawPath(path, p);
   // }

   // glClearColor(0.0, 0.0, 0.0, 1.0);
   // glClear(GL_COLOR_BUFFER_BIT);

   auto cnv = canvas{ gpuCanvas };
   cnv.scale(scale, scale);
   draw(cnv);


   // {
   //    cnv.scale(scale, scale);

   //    cnv.rect(50, 50, b.size.width-100, b.size.height-100);
   //    cnv.fill_style(colors::magenta);
   //    cnv.fill();
   // }


//   glClearColor(0.0, 0.0, 1.0, 1.0);
//   glClear(GL_COLOR_BUFFER_BIT);

   // TODO: Perform rendering here.

   [context flushBuffer];
   CGLUnlockContext(context.CGLContextObj);
}

@end

#endif // ARTIST_SKIA

//=======================================================================

@implementation CocoaView

- (void) start
{

#if defined(ARTIST_SKIA)

   // Enable retina-support
   self.wantsBestResolutionOpenGLSurface = YES;

   // Enable layer-backed drawing of view
   [self setWantsLayer:YES];

   self.layer.opaque = YES;

#endif
}

- (void) drawRect : (NSRect) dirty
{
   [super drawRect : dirty];

#if defined(ARTIST_QUARTZ_2D)
   auto ctx = NSGraphicsContext.currentContext.CGContext;
   auto cnv = canvas{ (host_context_ptr) ctx };
   draw(cnv);
#endif
}

#if defined(ARTIST_SKIA)

// - (void) render
// {
// //   const GrGLInterface* interface = nullptr;
// //   sk_sp<GrContext> context = GrContext::MakeGL(glInterface);
//    sk_sp<GrContext> context = GrContext::MakeGL(nullptr);
//    SkImageInfo info = SkImageInfo:: MakeN32Premul(640, 480); // $$$ HARD CODE !!! $$$

//    sk_sp<SkSurface> gpuSurface(
//       SkSurface::MakeRenderTarget(context.get(), SkBudgeted::kNo, info));

//    if (!gpuSurface)
//    {
//       SkDebugf("SkSurface::MakeRenderTarget returned null\n");
//       return;
//    }
//    SkCanvas* gpuCanvas = gpuSurface->getCanvas();

//    auto cnv = canvas{ gpuCanvas };
//    draw(cnv);
// }

- (CALayer*) makeBackingLayer
{
   return [[OpenGLLayer alloc] initWithIGraphicsView : self];
}

- (void) viewDidChangeBackingProperties
{
   [super viewDidChangeBackingProperties];

   // Need to propagate information about retina resolution
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
   // [self render];
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
