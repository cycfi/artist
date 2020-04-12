/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#pragma comment(lib, "dwrite")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "shcore.lib")

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <wchar.h>
#include <math.h>

#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wincodec.h>
#include <ShellScalingAPI.h>

template<class Interface>
inline void SafeRelease(Interface** ppInterfaceToRelease)
{
   if (*ppInterfaceToRelease != nullptr)
   {
      (*ppInterfaceToRelease)->Release();
      (*ppInterfaceToRelease) = nullptr;
   }
}

#ifndef HINST_THISCOMPONENT
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)
#endif

class DemoApp
{
public:
               ~DemoApp();

   HRESULT     Initialize();
   void        RunMessageLoop();

private:

   HRESULT     CreateDeviceIndependentResources();
   HRESULT     CreateDeviceResources();
   void        DiscardDeviceResources();
   HRESULT     OnRender();
   void        OnResize(UINT width, UINT height);

   static LRESULT CALLBACK WndProc(
      HWND hWnd,
      UINT message,
      WPARAM wParam,
      LPARAM lParam
   );

private:

   HWND                    _hwnd = nullptr;
   ID2D1Factory*           _pD2DFactory = nullptr;
   IWICImagingFactory*     _pWICFactory = nullptr;
   IDWriteFactory*         _pDWriteFactory = nullptr;
   ID2D1HwndRenderTarget*  _pRenderTarget = nullptr;
   IDWriteTextFormat*      _pTextFormat = nullptr;
   ID2D1SolidColorBrush*   _pBlackBrush = nullptr;
};

DemoApp::~DemoApp()
{
   SafeRelease(&_pD2DFactory);
   SafeRelease(&_pDWriteFactory);
   SafeRelease(&_pRenderTarget);
   SafeRelease(&_pTextFormat);
   SafeRelease(&_pBlackBrush);
}

HRESULT DemoApp::Initialize()
{
   HRESULT hr;

   // Initialize device-indpendent resources, such
   // as the Direct2D factory.
   hr = CreateDeviceIndependentResources();
   if (SUCCEEDED(hr))
   {
      // Register the window class.
      WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
      wcex.style         = CS_HREDRAW | CS_VREDRAW;
      wcex.lpfnWndProc   = DemoApp::WndProc;
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
      _pD2DFactory->GetDesktopDpi(&dpiX, &dpiY);

      _hwnd = CreateWindow(
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
      hr = _hwnd ? S_OK : E_FAIL;
      if (SUCCEEDED(hr))
      {
         ShowWindow(_hwnd, SW_SHOWNORMAL);

         UpdateWindow(_hwnd);
      }
   }

   return hr;
}

HRESULT DemoApp::CreateDeviceIndependentResources()
{
   static const WCHAR msc_fontName[] = L"Verdana";
   static const FLOAT msc_fontSize = 50;
   HRESULT hr;
   ID2D1GeometrySink *pSink = nullptr;

   // Create a Direct2D factory.
   hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &_pD2DFactory);

   if (SUCCEEDED(hr))
   {
      // Create a DirectWrite factory.
      hr = DWriteCreateFactory(
         DWRITE_FACTORY_TYPE_SHARED,
         __uuidof(_pDWriteFactory),
         reinterpret_cast<IUnknown**>(&_pDWriteFactory)
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

   SafeRelease(&pSink);

   return hr;
}

HRESULT DemoApp::CreateDeviceResources()
{
   HRESULT hr = S_OK;

   if (!_pRenderTarget)
   {
      RECT rc;
      GetClientRect(_hwnd, &rc);

      D2D1_SIZE_U size = D2D1::SizeU(
         rc.right - rc.left,
         rc.bottom - rc.top
         );

      // Create a Direct2D render target.
      hr = _pD2DFactory->CreateHwndRenderTarget(
         D2D1::RenderTargetProperties(),
         D2D1::HwndRenderTargetProperties(_hwnd, size),
         &_pRenderTarget
      );
      if (SUCCEEDED(hr))
      {
         // Create a black brush.
         hr = _pRenderTarget->CreateSolidColorBrush(
            D2D1::ColorF(D2D1::ColorF::Black),
            &_pBlackBrush
         );
      }
   }
   return hr;
}

void DemoApp::DiscardDeviceResources()
{
   SafeRelease(&_pRenderTarget);
   SafeRelease(&_pBlackBrush);
}

void DemoApp::RunMessageLoop()
{
   MSG msg;

   while (GetMessage(&msg, nullptr, 0, 0))
   {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
   }
}

HRESULT DemoApp::OnRender()
{
   HRESULT hr;

   hr = CreateDeviceResources();

   if (SUCCEEDED(hr) && !(_pRenderTarget->CheckWindowState() & D2D1_WINDOW_STATE_OCCLUDED))
   {
      static const WCHAR sc_helloWorld[] = L"Hello, World!";

      // Retrieve the size of the render target.
      D2D1_SIZE_F renderTargetSize = _pRenderTarget->GetSize();

      _pRenderTarget->BeginDraw();

      _pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

      _pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

      _pRenderTarget->DrawText(
         sc_helloWorld,
         ARRAYSIZE(sc_helloWorld) - 1,
         _pTextFormat,
         D2D1::RectF(0, 0, renderTargetSize.width, renderTargetSize.height),
         _pBlackBrush
         );

      hr = _pRenderTarget->EndDraw();

      if (hr == D2DERR_RECREATE_TARGET)
      {
         hr = S_OK;
         DiscardDeviceResources();
      }
   }

   return hr;
}

void DemoApp::OnResize(UINT width, UINT height)
{
   if (_pRenderTarget)
   {
      D2D1_SIZE_U size;
      size.width = width;
      size.height = height;

      // Note: This method can fail, but it's okay to ignore the
      // error here -- it will be repeated on the next call to
      // EndDraw.
      _pRenderTarget->Resize(size);
   }
}

LRESULT CALLBACK DemoApp::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
   LRESULT result = 0;

   if (message == WM_CREATE)
   {
      LPCREATESTRUCT pcs = (LPCREATESTRUCT)lParam;
      DemoApp *pDemoApp = (DemoApp *)pcs->lpCreateParams;

      ::SetWindowLongPtrW(
         hwnd,
         GWLP_USERDATA,
         reinterpret_cast<LONG_PTR>(pDemoApp)
         );

      result = 1;
   }
   else
   {
      DemoApp *pDemoApp = reinterpret_cast<DemoApp *>(
         ::GetWindowLongPtrW(
               hwnd,
               GWLP_USERDATA
               ));

      bool wasHandled = false;

      if (pDemoApp)
      {
         switch (message)
         {
         case WM_SIZE:
               {
                  UINT width = LOWORD(lParam);
                  UINT height = HIWORD(lParam);
                  pDemoApp->OnResize(width, height);
               }
               wasHandled = true;
               result = 0;
               break;

         case WM_PAINT:
         case WM_DISPLAYCHANGE:
               {
                  PAINTSTRUCT ps;
                  BeginPaint(hwnd, &ps);

                  pDemoApp->OnRender();
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
      {
         DemoApp app;

         if (SUCCEEDED(app.Initialize()))
         {
            app.RunMessageLoop();
         }
      }
      CoUninitialize();
   }

   return 0;
}

