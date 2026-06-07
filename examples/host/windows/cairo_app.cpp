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
#include <ShellScalingAPI.h>
#include <cairo.h>
#include <cairo-win32.h>
#include <stdexcept>
#include <chrono>

using namespace cycfi::artist;
float elapsed_ = 0;  // rendering elapsed time
constexpr unsigned IDT_TIMER1 = 100;

class window
{
public:

               window(extent size, color bkd, bool animate);
               ~window();

   void        render(HWND hwnd);
   void        on_dpi_changed(unsigned dpi, RECT const* suggested);

private:

   ATOM        registerClass(HINSTANCE hInstance);
   void        make_offscreen_dc(HDC hdc, int w, int h);

   extent      _size;
   bool        _animate;
   float       _scale = 1.0f;

   HDC         _hdc            = nullptr;
   HDC         _offscreen_hdc  = nullptr;
   HBITMAP     _offscreen_buff = nullptr;
   HWND        _wnd            = nullptr;
};

static window* g_window = nullptr;

void window::render(HWND hwnd)
{
   PAINTSTRUCT ps;
   HDC hdc = BeginPaint(hwnd, &ps);
   SetBkMode(hdc, TRANSPARENT);

   // Size the offscreen buffer to the CLIENT area (not the whole window incl.
   // title bar / borders) so the canvas clip_extent() matches the true drawable
   // region. Reflow examples read clip_extent() directly; static ones letterbox
   // against it. Update _size so the recreate check settles after a resize.
   RECT cr;
   GetClientRect(hwnd, &cr);
   int cw = cr.right  - cr.left;
   int ch = cr.bottom - cr.top;
   if (cw <= 0 || ch <= 0)
   {
      EndPaint(hwnd, &ps);
      return;
   }

   if (hdc != _hdc || cw != int(_size.x * _scale) || ch != int(_size.y * _scale))
   {
      make_offscreen_dc(hdc, cw, ch);
      _size = extent{float(cw / _scale), float(ch / _scale)};
   }

   HANDLE hold = SelectObject(_offscreen_hdc, _offscreen_buff);

   cairo_surface_t* surface = cairo_win32_surface_create(_offscreen_hdc);
   cairo_t* context = cairo_create(surface);
   cairo_scale(context, _scale, _scale);

   auto start = std::chrono::steady_clock::now();
   auto cnv = canvas{context};
   draw(cnv);
   auto stop = std::chrono::steady_clock::now();
   elapsed_ = std::chrono::duration<double>{stop - start}.count();

   cairo_destroy(context);
   cairo_surface_destroy(surface);

   // Blit the whole client area (resize + timer invalidate the full client).
   BitBlt(hdc, 0, 0, cw, ch, _offscreen_hdc, 0, 0, SRCCOPY);

   SelectObject(_offscreen_hdc, hold);
   EndPaint(hwnd, &ps);
}

void window::make_offscreen_dc(HDC hdc, int w, int h)
{
   if (_offscreen_buff) DeleteObject(_offscreen_buff);
   if (_offscreen_hdc)  DeleteDC(_offscreen_hdc);

   _hdc            = hdc;
   _offscreen_hdc  = CreateCompatibleDC(hdc);
   _offscreen_buff = CreateCompatibleBitmap(hdc, w, h);
}

void window::on_dpi_changed(unsigned dpi, RECT const* suggested)
{
   // The window moved to a monitor with a different scale factor. Update the
   // scale and adopt Windows' suggested rect; the WM_SIZE that follows repaints
   // (and recreates the offscreen buffer) at the new resolution.
   _scale = dpi / 96.0f;
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
            if (g_window)
               g_window->render(hwnd);
            break;

         case WM_SIZE:
            // Repaint the whole client at the new size. UpdateWindow forces a
            // synchronous WM_PAINT so content reflows live during the drag
            // (WM_SIZE arrives inside the modal resize loop). Skip minimize.
            if (g_window && wparam != SIZE_MINIMIZED)
            {
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
   wcex.lpszClassName = L"ArtistCairo";
   return RegisterClassEx(&wcex);
}

window::window(extent size, color /*bkd*/, bool animate)
 : _size{size}
 , _animate{animate}
{
   auto error = [](char const* msg){ throw std::runtime_error(msg); };

   // WS_OVERLAPPEDWINDOW adds WS_THICKFRAME (resize grips) + WS_MAXIMIZEBOX +
   // WS_MINIMIZEBOX, making the window resizable (was caption + sysmenu only).
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
}

window::~window()
{
   if (_offscreen_buff) DeleteObject(_offscreen_buff);
   if (_offscreen_hdc)  DeleteDC(_offscreen_hdc);
   if (_wnd)            DestroyWindow(_wnd);
   if (_animate)        KillTimer(_wnd, IDT_TIMER1);
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
   // Per-monitor-v2: render at the real pixel resolution and handle DPI changes
   // (WM_DPICHANGED) ourselves so Windows never bitmap-stretches the window.
   // System-DPI awareness (the old call) left 4K/scaled displays pixelated.
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
