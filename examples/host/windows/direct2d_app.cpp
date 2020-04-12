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

#include <canvas_impl.hpp>
#include <ShellScalingAPI.h>

#ifndef HINST_THISCOMPONENT
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)
#endif

namespace ca = cycfi::artist;

class app
{
public:
               app();
               ~app();

   void        run();

private:

   HRESULT     create_device_independent_resources();
   HRESULT     create_device_resources();
   void        discard_device_resources();
   HRESULT     render();
   void        resize(UINT width, UINT height);

   static LRESULT CALLBACK WndProc(
      HWND hWnd,
      UINT message,
      WPARAM wParam,
      LPARAM lParam
   );

private:

   using canvas_impl_ptr = std::unique_ptr<ca::canvas_impl>;

   canvas_impl_ptr         _canvas;

   ID2D1Factory*           _factory = nullptr;
   IWICImagingFactory*     _pWICFactory = nullptr;
   IDWriteFactory*         _pDWriteFactory = nullptr;
   IDWriteTextFormat*      _pTextFormat = nullptr;
   ID2D1SolidColorBrush*   _pBlackBrush = nullptr;
};

app::app()
{
   // Initialize device-indpendent resources, such
   // as the Direct2D factory.
   auto hr = create_device_independent_resources();
   if (SUCCEEDED(hr))
   {
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
      _factory->GetDesktopDpi(&dpiX, &dpiY);

      auto hwnd = CreateWindow(
         L"D2DDemoApp",
         L"Direct2D Demo Application",
         WS_OVERLAPPEDWINDOW,
         CW_USEDEFAULT,
         CW_USEDEFAULT,
         static_cast<UINT>(ceil(640.f * dpiX / 96.f)),
         static_cast<UINT>(ceil(480.f * dpiY / 96.f)),
         nullptr,
         nullptr,
         HINST_THISCOMPONENT,
         this
         );
      hr = hwnd ? S_OK : E_FAIL;

      if (SUCCEEDED(hr))
      {
         _canvas = std::make_unique<ca::canvas_impl>(hwnd, _factory);
         ShowWindow(_canvas->hwnd(), SW_SHOWNORMAL);
         UpdateWindow(_canvas->hwnd());
      }
   }

   if (!SUCCEEDED(hr))
      throw std::runtime_error{ "Error: failed to create app." };
}

app::~app()
{
   ca::release(_factory);
   ca::release(_pDWriteFactory);
   ca::release(_pTextFormat);
   ca::release(_pBlackBrush);
   CoUninitialize();
}

HRESULT app::create_device_independent_resources()
{
   static const WCHAR msc_fontName[] = L"Verdana";
   static const FLOAT msc_fontSize = 50;
   ID2D1GeometrySink *pSink = nullptr;

   HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, &_factory);
   if (SUCCEEDED(hr))
   {
      // Create a DirectWrite factory.
      hr = DWriteCreateFactory(
         DWRITE_FACTORY_TYPE_SHARED,
         __uuidof(_pDWriteFactory),
         reinterpret_cast<IUnknown **>(&_pDWriteFactory)
      );
   }

   if (SUCCEEDED(hr))
   {
      // Create a DirectWrite text format object.
      hr = _pDWriteFactory->CreateTextFormat(
         msc_fontName,
         nullptr,
         DWRITE_FONT_WEIGHT_NORMAL,
         DWRITE_FONT_STYLE_NORMAL,
         DWRITE_FONT_STRETCH_NORMAL,
         msc_fontSize,
         L"", //locale
         &_pTextFormat
      );
   }
   if (SUCCEEDED(hr))
   {
      // Center the text horizontally and vertically.
      _pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
      _pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
   }

   ca::release(pSink);

   return hr;
}

HRESULT app::create_device_resources()
{
   if (_canvas->update())
   {
      // Create a black brush.
      return _canvas->render_target()->CreateSolidColorBrush(
         D2D1::ColorF(D2D1::ColorF::Black),
         &_pBlackBrush
      );
   }
   return S_OK;
}

void app::discard_device_resources()
{
   _canvas->discard();
   ca::release(_pBlackBrush);
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

HRESULT app::render()
{
   HRESULT hr;

   hr = create_device_resources();
   auto render_target = _canvas->render_target();

   if (SUCCEEDED(hr) && !(render_target->CheckWindowState() & D2D1_WINDOW_STATE_OCCLUDED))
   {
      static const WCHAR sc_helloWorld[] = L"Hello, World!";

      // Retrieve the size of the render target.
      auto size = render_target->GetSize();

      render_target->BeginDraw();

      render_target->SetTransform(D2D1::Matrix3x2F::Identity());

      render_target->Clear(D2D1::ColorF(D2D1::ColorF::White));

      render_target->DrawText(
         sc_helloWorld,
         ARRAYSIZE(sc_helloWorld) - 1,
         _pTextFormat,
         D2D1::RectF(0, 0, size.width, size.height),
         _pBlackBrush
         );

      hr = render_target->EndDraw();

      if (hr == D2DERR_RECREATE_TARGET)
      {
         hr = S_OK;
         discard_device_resources();
      }
   }

   return hr;
}

void app::resize(UINT width, UINT height)
{
   if (_canvas->render_target())
   {
      D2D1_SIZE_U size;
      size.width = width;
      size.height = height;

      // Note: This method can fail, but it's okay to ignore the
      // error here -- it will be repeated on the next call to
      // EndDraw.
      _canvas->render_target()->Resize(size);
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

int main(int argc, char const* argv[])
{
   SetProcessDpiAwareness(PROCESS_SYSTEM_DPI_AWARE);

   // Ignore the return value because we want to run the program
   // even in the unlikely event that HeapSetInformation fails.
   HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);

   if (SUCCEEDED(CoInitialize(nullptr)))
   {
      app _app;
      _app.run();
   }
   return 0;
}

