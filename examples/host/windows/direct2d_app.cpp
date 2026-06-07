/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Direct2D example host. Mirrors the resizable cairo_app.cpp / skia_app.cpp
   structure (Win32 window, WM_SIZE live redraw, DPI scale) but renders with
   Direct2D: an ID2D1HwndRenderTarget wrapped per-frame in a d2d::context that
   becomes the canvas_impl. Device loss is handled at EndDraw
   (D2DERR_RECREATE_TARGET -> drop the target, recreate on the next paint).
=============================================================================*/
#include "../../app.hpp"

#ifndef UNICODE
#define UNICODE
#endif

#include <SDKDDKVer.h>
#include <windows.h>
#include <ShellScalingAPI.h>
#include <context.hpp>          // d2d::context, get_factory (backend impl header)
#include <stdexcept>
#include <chrono>

using namespace cycfi::artist;
namespace d2d = cycfi::artist::d2d;

float elapsed_ = 0;  // rendering elapsed time
constexpr unsigned IDT_TIMER1 = 100;

class window
{
public:

               window(extent size, color bkd, bool animate);
               ~window();

   void        render(HWND hwnd);
   void        on_resize();
   void        on_dpi_changed(unsigned dpi, RECT const* suggested);

private:

   ATOM        registerClass(HINSTANCE hInstance);
   bool        create_target();

   extent      _size;
   color       _bkd;
   bool        _animate;
   float       _scale = 1.0f;

   ID2D1HwndRenderTarget* _target = nullptr;
   HWND        _wnd = nullptr;
};

static window* g_window = nullptr;

bool window::create_target()
{
   if (_target)
      return true;

   RECT cr;
   GetClientRect(_wnd, &cr);
   D2D1_SIZE_U px = D2D1::SizeU(cr.right - cr.left, cr.bottom - cr.top);
   if (px.width == 0 || px.height == 0)
      return false;

   // Set the render target DPI to 96*scale so 1 DIP == 1 logical pixel and
   // Direct2D applies the HiDPI scale itself; the canvas then draws in logical
   // coordinates (clip_extent() returns the logical size via GetSize()).
   auto props = D2D1::RenderTargetProperties(
      D2D1_RENDER_TARGET_TYPE_DEFAULT,
      D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
      96.0f * _scale, 96.0f * _scale
   );

   auto hr = d2d::get_factory().CreateHwndRenderTarget(
      props,
      D2D1::HwndRenderTargetProperties(_wnd, px),
      &_target
   );
   return SUCCEEDED(hr);
}

void window::render(HWND hwnd)
{
   PAINTSTRUCT ps;
   BeginPaint(hwnd, &ps);

   if (create_target())
   {
      _target->BeginDraw();
      _target->SetTransform(D2D1::Matrix3x2F::Identity());
      _target->Clear(D2D1::ColorF(_bkd.red, _bkd.green, _bkd.blue, _bkd.alpha));

      auto start = std::chrono::steady_clock::now();
      {
         d2d::context ctx{_target};
         auto cnv = canvas{&ctx};
         draw(cnv);
      }
      auto stop = std::chrono::steady_clock::now();
      elapsed_ = std::chrono::duration<double>{stop - start}.count();

      auto hr = _target->EndDraw();
      if (hr == D2DERR_RECREATE_TARGET)
      {
         // Device loss: drop the device-dependent target; the next WM_PAINT
         // recreates it and redraws from scratch.
         d2d::release(_target);
      }
   }

   EndPaint(hwnd, &ps);
}

void window::on_resize()
{
   // Resize the back buffer to the new client size so the next frame is drawn
   // (as vectors) at the new resolution. Without this, the HwndRenderTarget
   // keeps its original size and Direct2D bitmap-stretches it -> blurry/pixelated.
   if (!_target)
      return;
   RECT cr;
   GetClientRect(_wnd, &cr);
   _target->Resize(D2D1::SizeU(cr.right - cr.left, cr.bottom - cr.top));
}

void window::on_dpi_changed(unsigned dpi, RECT const* suggested)
{
   // The window moved to a monitor with a different scale factor. Update the
   // render-target DPI (so 1 DIP stays 1 logical pixel and content stays crisp)
   // and adopt Windows' suggested position/size; the WM_SIZE that follows
   // resizes the back buffer.
   _scale = dpi / 96.0f;
   if (_target)
      _target->SetDpi(96.0f * _scale, 96.0f * _scale);
   if (suggested)
      SetWindowPos(
         _wnd, nullptr,
         suggested->left, suggested->top,
         suggested->right - suggested->left,
         suggested->bottom - suggested->top,
         SWP_NOZORDER | SWP_NOACTIVATE);
   InvalidateRect(_wnd, nullptr, FALSE);
}

namespace
{
   LRESULT CALLBACK handle_event(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
   {
      switch (message)
      {
         case WM_TIMER:
            if (wparam == IDT_TIMER1 && g_window)
               InvalidateRect(hwnd, nullptr, false);
            break;

         case WM_PAINT:
         case WM_DISPLAYCHANGE:
            if (g_window)
               g_window->render(hwnd);
            break;

         case WM_SIZE:
            if (g_window && wparam != SIZE_MINIMIZED)
            {
               g_window->on_resize();
               InvalidateRect(hwnd, nullptr, FALSE);
               UpdateWindow(hwnd);
            }
            break;

         case WM_DPICHANGED:
            if (g_window)
               g_window->on_dpi_changed(
                  HIWORD(wparam), reinterpret_cast<RECT const*>(lparam));
            break;

         case WM_KEYDOWN:
            if (wparam == VK_ESCAPE)
            {
               KillTimer(hwnd, IDT_TIMER1);
               PostQuitMessage(0);
            }
            break;

         case WM_CLOSE:
            KillTimer(hwnd, IDT_TIMER1);
            PostQuitMessage(0);
            break;

         default:
            return DefWindowProc(hwnd, message, wparam, lparam);
      }
      return 0;
   }
}

ATOM window::registerClass(HINSTANCE hInstance)
{
   WNDCLASSEX wcex;
   ZeroMemory(&wcex, sizeof(wcex));
   wcex.cbSize        = sizeof(wcex);
   wcex.style         = CS_HREDRAW | CS_VREDRAW;
   wcex.lpfnWndProc   = handle_event;
   wcex.hInstance     = hInstance;
   wcex.hCursor       = LoadCursor(nullptr, IDC_ARROW);
   wcex.lpszClassName = L"ArtistDirect2D";
   return RegisterClassEx(&wcex);
}

window::window(extent size, color bkd, bool animate)
 : _size{size}
 , _bkd{bkd}
 , _animate{animate}
{
   auto error = [](char const* msg){ throw std::runtime_error(msg); };

   auto style = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
   LPTSTR windowClass = MAKEINTATOM(registerClass(nullptr));
   if (windowClass == 0)
      error("Error: registerClass() failed.");

   _wnd = CreateWindow(
      windowClass, L"Artist",
      style,
      CW_USEDEFAULT, CW_USEDEFAULT,
      1, 1,
      nullptr, nullptr, nullptr, nullptr
   );
   if (!_wnd)
      error("Error: CreateWindow() failed.");

   auto dpi = GetDpiForWindow(_wnd);
   _scale = dpi / 96.0f;
   float px = size.x * _scale;
   float py = size.y * _scale;

   RECT frame;
   GetWindowRect(_wnd, &frame);
   RECT rect = {0, 0, LONG(px), LONG(py)};
   AdjustWindowRectExForDpi(&rect, style, false, WS_EX_APPWINDOW, dpi);
   frame.right  = frame.left + (rect.right  - rect.left);
   frame.bottom = frame.top  + (rect.bottom - rect.top);

   MoveWindow(_wnd, frame.left, frame.top,
              frame.right - frame.left, frame.bottom - frame.top, false);

   g_window = this;

   if (animate)
      SetTimer(_wnd, IDT_TIMER1, 16, (TIMERPROC) nullptr);

   ShowWindow(_wnd, SW_SHOW);
   UpdateWindow(_wnd);
}

window::~window()
{
   d2d::release(_target);
   if (_animate && _wnd) KillTimer(_wnd, IDT_TIMER1);
   if (_wnd)             DestroyWindow(_wnd);
   g_window = nullptr;
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
   (void)argc; (void)argv;
   // Per-monitor-v2: we render at the real pixel resolution and handle DPI
   // ourselves (target DPI + WM_DPICHANGED), so Windows never bitmap-stretches
   // the window. System-DPI awareness (the old call) left 4K/scaled displays
   // virtualized and pixelated.
   SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
   window win{window_size, bkd, animate};

   MSG msg;
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
   return (int)msg.wParam;
}
