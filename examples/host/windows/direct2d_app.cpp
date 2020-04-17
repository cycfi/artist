/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#ifndef UNICODE
#define UNICODE
#endif

#pragma comment(lib, "dwrite")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "shcore.lib")
#pragma comment(lib, "dxguid.lib")

#include "../../app.hpp"
#include <context.hpp>
#include <ShellScalingAPI.h>

#ifndef HINST_THISCOMPONENT
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)
#endif

namespace ca = cycfi::artist;

class app
{
public:
               app(extent size, color bkd, bool animate);
               ~app();

   void        run();

private:

   void        render();
   void        resize(UINT width, UINT height);

   static LRESULT CALLBACK WndProc(
      HWND hWnd,
      UINT message,
      WPARAM wParam,
      LPARAM lParam
   );

private:

   using context_ptr = std::unique_ptr<ca::d2d::context>;
   using canvas_ptr = std::unique_ptr<ca::canvas>;

   context_ptr       _canvas_impl;
   canvas_ptr        _canvas;
};

app::app(extent size, color bkd, bool animate)
{
   auto hr = S_OK;

   // Register the window class.
   WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
   wcex.style         = CS_HREDRAW | CS_VREDRAW;
   wcex.lpfnWndProc   = app::WndProc;
   wcex.cbClsExtra    = 0;
   wcex.cbWndExtra    = sizeof(LONG_PTR);
   wcex.hInstance     = HINST_THISCOMPONENT;
   wcex.hbrBackground = nullptr;
   wcex.lpszMenuName  = nullptr;
   wcex.hCursor       = LoadCursor(nullptr, IDC_ARROW);
   wcex.lpszClassName = L"D2DDemoApp";

   RegisterClassEx(&wcex);

   // Create the application window.
   //
   // Because the CreateWindow function takes its size in pixels, we
   // obtain the system DPI and use it to scale the window size.
   FLOAT dpiX, dpiY;
   ca::d2d::get_factory().GetDesktopDpi(&dpiX, &dpiY);
   auto style = /*WS_OVERLAPPEDWINDOW; */ WS_CAPTION | WS_SYSMENU | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

   size.x *= dpiX / 96.f;
   size.y *= dpiY / 96.f;

   RECT rect = { 0, 0, LONG(size.x), LONG(size.y) };
   AdjustWindowRectExForDpi(&rect, style, false, WS_EX_APPWINDOW, dpiX);

   auto hwnd = CreateWindow(
      L"D2DDemoApp",
      L"Direct2D",
      style,
      CW_USEDEFAULT, CW_USEDEFAULT,       // default position
      rect.right-rect.left,               // width
      rect.bottom-rect.top,               // height
      nullptr, nullptr,                   // parent window, menu
      HINST_THISCOMPONENT,
      this
      );
   hr = hwnd ? S_OK : E_FAIL;

   if (SUCCEEDED(hr))
   {
      _canvas_impl = std::make_unique<ca::d2d::context>(hwnd, bkd);
      _canvas = std::make_unique<ca::canvas>(_canvas_impl.get());

      ShowWindow(_canvas_impl->hwnd(), SW_SHOWNORMAL);
      UpdateWindow(_canvas_impl->hwnd());
   }

   if (!SUCCEEDED(hr))
      throw std::runtime_error{ "Error: failed to create app." };
}

app::~app()
{
   CoUninitialize();
}

void app::run()
{
   MSG msg;

   while (GetMessage(&msg, nullptr, 0, 0))
   {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
   }
}

void app::render()
{
   _canvas_impl->render(
      [this](auto&)
      {
         draw(*_canvas);
      }
   );
}

void app::resize(UINT width, UINT height)
{
   if (_canvas_impl->target())
   {
      D2D1_SIZE_U size;
      size.width = width;
      size.height = height;

      // Note: This method can fail, but it's okay to ignore the
      // error here -- it will be repeated on the next call to
      // EndDraw.
      // $$$ for now $$$ need dynamic cast _canvas_impl->canvas()->Resize(size);
   }
}

LRESULT CALLBACK app::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
   LRESULT result = 0;

   if (message == WM_CREATE)
   {
      LPCREATESTRUCT pcs = (LPCREATESTRUCT)lParam;
      app* _app = (app*)pcs->lpCreateParams;

      ::SetWindowLongPtrW(
         hwnd,
         GWLP_USERDATA,
         reinterpret_cast<LONG_PTR>(_app)
      );

      result = 1;
   }
   else
   {
      app* _app = reinterpret_cast<app *>(
         ::GetWindowLongPtrW(
               hwnd,
               GWLP_USERDATA
         ));

      bool wasHandled = false;

      if (_app)
      {
         switch (message)
         {
            case WM_SIZE:
               {
                  UINT width = LOWORD(lParam);
                  UINT height = HIWORD(lParam);
                  _app->resize(width, height);
               }
               wasHandled = true;
               result = 0;
               break;

            case WM_PAINT:
            case WM_DISPLAYCHANGE:
               {
                  PAINTSTRUCT ps;
                  BeginPaint(hwnd, &ps);

                  _app->render();
                  EndPaint(hwnd, &ps);
               }
               wasHandled = true;
               result = 0;
               break;

            case WM_DESTROY:
               {
                  PostQuitMessage(0);
               }
               wasHandled = true;
               result = 1;
               break;
         }
      }

      if (!wasHandled)
      {
         result = DefWindowProc(hwnd, message, wParam, lParam);
      }
   }

   return result;
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

   // Ignore the return value because we want to run the program
   // even in the unlikely event that HeapSetInformation fails.
   HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);

   if (SUCCEEDED(CoInitialize(nullptr)))
   {
      app _app{ window_size, bkd, animate };
      _app.run();
   }
   return 0;
}

