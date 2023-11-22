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
#include <gl/gl.h>

#include <GrContext.h>
#include <gl/GrGLInterface.h>
#include <SkImage.h>
#include <SkSurface.h>
#include <tools/sk_app/DisplayParams.h>
#include <tools/sk_app/WindowContext.h>
#include <tools/sk_app/win/WindowContextFactory_win.h>

#include <ShellScalingAPI.h>

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

   void           render();

private:

   ATOM           registerClass(HINSTANCE hInstance);
   void           destroy();

   using skia_context = std::unique_ptr<sk_app::WindowContext>;

   extent         _size;
   bool           _animate;
   skia_context   _skia_context;
   float          _scale;

   HGLRC          RC;         // Rendering Context
   HDC            DC;         // Device Context
   HWND           WND;        // Window
};

window::window(extent size, color bkd, bool animate)
 : _size{size}
 , _animate{animate}
{
   auto error = [](char const* msg){ throw std::runtime_error(msg); };
   auto style = WS_CAPTION | WS_SYSMENU | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

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

   _skia_context = sk_app::window_context_factory::MakeGLForWin(WND, sk_app::DisplayParams());
   if (animate)
      SetTimer(WND, IDT_TIMER1, 16, (TIMERPROC) nullptr);

   SetWindowText(WND, L"Artist");
   ShowWindow(WND, SW_SHOW);
}

window::~window()
{
   wglMakeCurrent(nullptr, nullptr);
   if (RC)
      wglDeleteContext(RC);
   if (DC)
      ReleaseDC(WND, DC);
   if (WND)
      DestroyWindow(WND);
   if (_animate)
      KillTimer(WND, IDT_TIMER1);
}

LRESULT CALLBACK handle_event(
   HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
   auto param = GetWindowLongPtrW(hWnd, GWLP_USERDATA);
   auto win = reinterpret_cast<window*>(param);

   switch (message)
   {
      case WM_TIMER:
         if (wParam == IDT_TIMER1)
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
   sk_sp<SkSurface> surface = _skia_context->getBackbufferSurface();
   if (surface)
   {
      SkCanvas* gpu_canvas = surface->getCanvas();
      gpu_canvas->save();
      auto cnv = canvas{gpu_canvas};
      cnv.pre_scale(_scale);

      draw(cnv);

      gpu_canvas->restore();
      surface->flush();
      _skia_context->swapBuffers();
   }
}

void window::destroy()
{
   wglMakeCurrent(nullptr, nullptr);
   if (RC)
      wglDeleteContext(RC);
   if (DC)
      ReleaseDC(WND, DC);
   if (WND)
      DestroyWindow(WND);
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
   SetProcessDpiAwareness(PROCESS_SYSTEM_DPI_AWARE);
   window win{window_size, bkd, animate};

   MSG msg;
   bool active = true;
   int i = 0;
   float acc = 0;
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

   return msg.wParam;
}



