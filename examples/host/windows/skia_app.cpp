/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "../../app.hpp"

#ifndef UNICODE
#define UNICODE
#endif

#include <SDKDDKVer.h>
#include <windows.h>
#include <GL/gl.h>

// Windows' <GL/gl.h> is frozen at OpenGL 1.1, so tokens introduced in later
// versions are absent. Define the few we need (used only as constant args to
// glGetIntegerv / Skia's GrGLFramebufferInfo) rather than pull in <GL/glext.h>.
#ifndef GL_RGBA8
# define GL_RGBA8 0x8058
#endif
#ifndef GL_FRAMEBUFFER_BINDING
# define GL_FRAMEBUFFER_BINDING 0x8CA6
#endif

#include <SkImage.h>
#include <SkColorSpace.h>
#include <SkCanvas.h>
#include <SkSurface.h>
#include <ganesh/GrDirectContext.h>
#include <ganesh/GrBackendSurface.h>
#include <ganesh/SkSurfaceGanesh.h>
#include <ganesh/gl/GrGLInterface.h>
#include <ganesh/gl/GrGLDirectContext.h>
#include <ganesh/gl/GrGLBackendSurface.h>
#include <ganesh/gl/GrGLTypes.h>

#include <ShellScalingAPI.h>

#include <stdexcept>
#include <chrono>

using namespace cycfi::artist;
float elapsed_ = 0;  // rendering elapsed time
constexpr unsigned IDT_TIMER1 = 100;

// WGL ARB context-creation bits (avoid depending on <GL/wglext.h>). These let
// us request a core-profile OpenGL 3.3 context, which Skia's Ganesh GL backend
// targets. We fall back to the legacy wglCreateContext context if the ARB
// extension is unavailable (desktop drivers usually return a high-version
// compatibility context there anyway).
#define WGL_CONTEXT_MAJOR_VERSION_ARB    0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB    0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB     0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
using PFNWGLCREATECONTEXTATTRIBSARBPROC =
   HGLRC (WINAPI*)(HDC, HGLRC, const int*);

class window
{
public:

                  window(extent size, color bkd, bool animate);
                  ~window();

   void           render();
   void           on_resize(int w_px, int h_px);
   void           on_dpi_changed(unsigned dpi, RECT const* suggested);

private:

   ATOM           registerClass(HINSTANCE hInstance);
   void           make_gl_context();
   void           init_skia();

   extent         _size;
   bool           _animate;
   float          _scale;
   color          _bkd;

   HGLRC          RC = nullptr;   // Rendering Context
   HDC            DC = nullptr;   // Device Context
   HWND           WND = nullptr;  // Window

   sk_sp<const GrGLInterface> _xface;
   sk_sp<GrDirectContext>     _ctx;
   sk_sp<SkSurface>           _surface;
};

window::window(extent size, color bkd, bool animate)
 : _size{size}
 , _animate{animate}
 , _bkd{bkd}
{
   auto error = [](char const* msg){ throw std::runtime_error(msg); };
   // WS_OVERLAPPEDWINDOW adds WS_THICKFRAME (resize grips) + WS_MAXIMIZEBOX +
   // WS_MINIMIZEBOX, making the window resizable (was caption + sysmenu only).
   auto style = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

   LPTSTR windowClass = MAKEINTATOM(registerClass(nullptr));
   if (windowClass == 0)
      error("Error: registerClass() failed.");

   WND = CreateWindow(
      windowClass, L"Artist",
      style,
      CW_USEDEFAULT, CW_USEDEFAULT, // position x, y
      1, 1,                         // width, height
      nullptr, nullptr,             // parent window, menu
      nullptr, nullptr              // instance, param
   );

   auto dpi = GetDpiForWindow(WND);
   _scale = dpi / 96.0;
   size.x *= _scale;
   size.y *= _scale;

   RECT frame;
   GetWindowRect(WND, &frame);
   RECT rect = {0, 0, LONG(size.x), LONG(size.y)};
   AdjustWindowRectExForDpi(&rect, style, false, WS_EX_APPWINDOW, dpi);
   frame.right = frame.left + (rect.right-rect.left);
   frame.bottom = frame.top + (rect.bottom-rect.top);

   MoveWindow(
      WND, frame.left, frame.top,
      frame.right - frame.left,
      frame.bottom - frame.top,
      false
   );

   SetWindowLongPtrW(WND, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

   make_gl_context();
   init_skia();

   if (animate)
      SetTimer(WND, IDT_TIMER1, 16, (TIMERPROC) nullptr);

   SetWindowText(WND, L"Artist");
   ShowWindow(WND, SW_SHOW);
}

window::~window()
{
   if (_animate && WND)
      KillTimer(WND, IDT_TIMER1);

   _surface.reset();
   _ctx.reset();
   _xface.reset();

   wglMakeCurrent(nullptr, nullptr);
   if (RC)
      wglDeleteContext(RC);
   if (DC)
      ReleaseDC(WND, DC);
   if (WND)
      DestroyWindow(WND);
}

// Create a WGL OpenGL context on the window's device context. Sets a double-
// buffered RGBA pixel format, makes a legacy context, then upgrades to a
// core-profile 3.3 context via wglCreateContextAttribsARB when available.
void window::make_gl_context()
{
   auto error = [](char const* msg){ throw std::runtime_error(msg); };

   DC = GetDC(WND);
   if (!DC)
      error("Error: GetDC failed.");

   PIXELFORMATDESCRIPTOR pfd = {};
   pfd.nSize = sizeof(pfd);
   pfd.nVersion = 1;
   pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
   pfd.iPixelType = PFD_TYPE_RGBA;
   pfd.cColorBits = 32;
   pfd.cAlphaBits = 8;
   pfd.cDepthBits = 24;
   pfd.cStencilBits = 8;
   pfd.iLayerType = PFD_MAIN_PLANE;

   int pf = ChoosePixelFormat(DC, &pfd);
   if (pf == 0 || !SetPixelFormat(DC, pf, &pfd))
      error("Error: SetPixelFormat failed.");

   HGLRC legacy = wglCreateContext(DC);
   if (!legacy)
      error("Error: wglCreateContext failed.");
   wglMakeCurrent(DC, legacy);

   auto wglCreateContextAttribsARB =
      reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(
         wglGetProcAddress("wglCreateContextAttribsARB"));

   if (wglCreateContextAttribsARB)
   {
      const int attribs[] =
      {
         WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
         WGL_CONTEXT_MINOR_VERSION_ARB, 3,
         WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
         0
      };
      if (HGLRC core = wglCreateContextAttribsARB(DC, nullptr, attribs))
      {
         wglMakeCurrent(DC, core);
         wglDeleteContext(legacy);
         RC = core;
      }
   }
   if (!RC)
      RC = legacy;   // fall back to the legacy (compatibility) context
}

void window::init_skia()
{
   auto error = [](char const* msg){ throw std::runtime_error(msg); };

   glClearColor(_bkd.red, _bkd.green, _bkd.blue, _bkd.alpha);
   glClear(GL_COLOR_BUFFER_BIT);

   if (_xface = GrGLMakeNativeInterface(); _xface == nullptr)
      error("Error: GrGLMakeNativeInterface failed.");
   if (_ctx = GrDirectContexts::MakeGL(_xface); _ctx == nullptr)
      error("Error: GrDirectContexts::MakeGL failed.");
}

LRESULT CALLBACK handle_event(
   HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
   auto param = GetWindowLongPtrW(hWnd, GWLP_USERDATA);
   auto win = reinterpret_cast<window*>(param);

   switch (message)
   {
      case WM_TIMER:
         if (wParam == IDT_TIMER1 && win)
         {
            auto start = std::chrono::steady_clock::now();
            win->render();
            auto stop = std::chrono::steady_clock::now();
            elapsed_ = std::chrono::duration<double>{stop - start}.count();
         }
         break;

      case WM_PAINT:
         if (win)
         {
            PAINTSTRUCT ps;
            BeginPaint(hWnd, &ps);
            win->render();
            EndPaint(hWnd, &ps);
         }
         break;

      case WM_SIZE:
         // lParam carries the new client size in physical pixels. Recreate the
         // Skia surface at that size and redraw immediately so content reflows
         // live during the drag (WM_SIZE is dispatched inside the modal resize
         // loop). Ignore minimize (zero size).
         if (win && wParam != SIZE_MINIMIZED)
            win->on_resize(LOWORD(lParam), HIWORD(lParam));
         break;

      case WM_DPICHANGED:
         // Moved to a monitor with a different scale factor. Update the scale and
         // adopt Windows' suggested rect; the WM_SIZE that follows recreates the
         // surface at the new resolution.
         if (win)
            win->on_dpi_changed(
               HIWORD(wParam), reinterpret_cast<RECT const*>(lParam));
         break;

      case WM_KEYDOWN:
         if (wParam == VK_ESCAPE)
         {
            KillTimer(hWnd, IDT_TIMER1);
            PostQuitMessage(0);
         }
         break;

      case WM_CLOSE:
         KillTimer(hWnd, IDT_TIMER1);
         PostQuitMessage(0);
         break;

      default:
         return DefWindowProc(hWnd, message, wParam, lParam);
   }
   return 0;
}

ATOM window::registerClass(HINSTANCE hInstance)
{
   WNDCLASSEX wcex;
   ZeroMemory(&wcex, sizeof(wcex));
   wcex.cbSize = sizeof(wcex);
   wcex.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
   wcex.lpfnWndProc = handle_event;
   wcex.hInstance = hInstance;
   wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
   wcex.lpszClassName = L"Core";
   return RegisterClassEx(&wcex);
}

void window::render()
{
   auto error = [](char const* msg){ throw std::runtime_error(msg); };
   wglMakeCurrent(DC, RC);

   if (!_surface)
   {
      GrGLint buffer = 0;
      glGetIntegerv(GL_FRAMEBUFFER_BINDING, &buffer);
      GrGLFramebufferInfo info;
      info.fFBOID = (GrGLuint) buffer;
      info.fFormat = GL_RGBA8;
      SkColorType colorType = kRGBA_8888_SkColorType;

      auto target = GrBackendRenderTargets::MakeGL(
         int(_size.x*_scale), int(_size.y*_scale), 0, 8, info);

      _surface = SkSurfaces::WrapBackendRenderTarget(
         _ctx.get(), target, kBottomLeft_GrSurfaceOrigin,
         colorType, nullptr, nullptr);

      if (!_surface)
         error("Error: SkSurfaces::WrapBackendRenderTarget returned null.");
   }

   SkCanvas* gpu_canvas = _surface->getCanvas();
   gpu_canvas->save();
   gpu_canvas->scale(_scale, _scale);
   auto cnv = canvas{gpu_canvas};

   draw(cnv);

   gpu_canvas->restore();
   _ctx->flushAndSubmit(_surface.get());
   SwapBuffers(DC);
}

// Handle a window resize: store the new logical size, drop the Skia surface so
// render() recreates it at the new client size (the WGL default framebuffer
// tracks the client area), and redraw immediately for responsiveness.
void window::on_resize(int w_px, int h_px)
{
   if (w_px <= 0 || h_px <= 0)
      return;
   _size = extent{float(w_px / _scale), float(h_px / _scale)};
   _surface.reset();
   render();
}

void window::on_dpi_changed(unsigned dpi, RECT const* suggested)
{
   _scale = dpi / 96.0f;
   if (suggested)
      SetWindowPos(
         WND, nullptr,
         suggested->left, suggested->top,
         suggested->right - suggested->left,
         suggested->bottom - suggested->top,
         SWP_NOZORDER | SWP_NOACTIVATE);
   // The WM_SIZE from the move recreates the surface at the new scale.
}

namespace cycfi::artist
{
   void add_relative_paths(const fs::path& base, const fs::path& rel_path)
   {
      add_search_path(base / rel_path);
      add_search_path(base.parent_path() / rel_path);
   }

   void init_paths()
   {
      fs::path curr_dir = fs::current_path();

      add_relative_paths(curr_dir, "resources");
      add_relative_paths(curr_dir, "resources/fonts");
      add_relative_paths(curr_dir, "resources/images");
   }

   // This is declared in font.hpp
   fs::path get_user_fonts_directory()
   {
      const fs::path fonts_dir = find_directory("fonts");
      if (fs::exists(fonts_dir))
         return fonts_dir;
      return fs::path(fs::current_path() / "resources/fonts");
   }
}

int run_app(
   int argc
 , char const* argv[]
 , extent window_size
 , color bkd
 , bool animate
)
{
   // Per-monitor-v2: render at the real pixel resolution and handle DPI changes
   // (WM_DPICHANGED) ourselves so Windows never bitmap-stretches the window.
   // System-DPI awareness (the old call) left 4K/scaled displays pixelated.
   SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
   window win{window_size, bkd, animate};

   MSG msg = {};
   bool active = true;
   while (active)
   {
      while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
      {
         if (msg.message == WM_QUIT)
            active = false;
         TranslateMessage(&msg);
         DispatchMessage(&msg);
      }
   }

   return int(msg.wParam);
}
