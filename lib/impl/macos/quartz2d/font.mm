/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/font.hpp>
#include <Quartz/Quartz.h>
#include <algorithm>
#include <sstream>
#include <cmath>
#include "osx_utils.hpp"
#include <dlfcn.h>
#include <infra/assert.hpp>
#include <artist/resources.hpp>

///////////////////////////////////////////////////////////////////////////////
// Helper utils
namespace
{
   namespace fs = cycfi::fs;

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

   void activate_font(fs::path font_path)
   {
      auto furl = [NSURL fileURLWithPath : [NSString stringWithUTF8String : font_path.c_str()]];
      CYCFI_ASSERT(furl, "Error: Unexpected missing font.");

      CFErrorRef error = nullptr;
      CTFontManagerRegisterFontsForURL((__bridge CFURLRef) furl, kCTFontManagerScopeProcess, &error);
   }

   void get_resource_path(char resource_path[])
   {
      CFBundleRef main_bundle = get_current_bundle();
      CFURLRef resources_url = CFBundleCopyResourcesDirectoryURL(main_bundle);
      CFURLGetFileSystemRepresentation(resources_url, TRUE, (UInt8*) resource_path, PATH_MAX);
      CFRelease(resources_url);
      CFRelease(main_bundle);
   }

   struct resource_setter
   {
      resource_setter()
      {
         // Before anything else, set the working directory so we can access
         // our resources
         char resource_path[PATH_MAX];
         get_resource_path(resource_path);
         cycfi::artist::add_search_path(resource_path);

         // Load the user fonts from the Resource folder. Normally this is automatically
         // done on application startup, but for plugins, we need to explicitly load
         // the user fonts ourself.
         for (fs::directory_iterator it{resource_path}; it != fs::directory_iterator{}; ++it)
            if (it->path().extension() == ".ttf")
               activate_font(it->path());
      }
   };
}

namespace cycfi::artist
{
   namespace
   {
      inline void ltrim(std::string& s)
      {
         s.erase(s.begin(), std::find_if(s.begin(), s.end(),
            [](int ch) { return ch != ' ' && ch != '"'; }
         ));
      }

      inline void rtrim(std::string& s)
      {
         s.erase(std::find_if(s.rbegin(), s.rend(),
            [](int ch) { return ch != ' ' && ch != '"'; }
         ).base(), s.end());
      }

      inline void trim(std::string& s)
      {
         ltrim(s);
         rtrim(s);
      }
   }

   using namespace font_constants;

   font::font()
    : _ptr(nullptr)
   {
   }

   font::font(font_descr descr)
    : _ptr(nullptr)
   {
      static resource_setter init_resources;

      int weight = std::ceil(float(descr._weight * 5) / 40);
      int style = 0;

      if (descr._slant)
         style |= NSItalicFontMask;
      if (descr._stretch <= condensed)
         style |= NSCondensedFontMask;
      else if (descr._stretch >= expanded)
         style |= NSExpandedFontMask;

      std::istringstream str(std::string{descr._families});
      std::string family;
      auto font_manager = [NSFontManager sharedFontManager];

      while (getline(str, family, ','))
      {
         trim(family);
         auto  family_ = [NSString stringWithUTF8String : family.c_str()];

         auto font =
            [font_manager
               fontWithFamily : family_
                       traits : style
                       weight : weight
                         size : descr._size
            ];

         if (font)
         {
            _ptr = (__bridge_retained font_impl_ptr) font;
            break;
         }
      }
      if (_ptr == nullptr)
      {
         _ptr = (__bridge_retained font_impl_ptr)
            [NSFont
               systemFontOfSize : descr._size
                         weight : weight
            ];
      }
   }

   font::font(font const& rhs)
    : _ptr((font_impl_ptr) CFRetain(rhs._ptr))
   {
   }

   font::font(font&& rhs) noexcept
    : _ptr(rhs._ptr)
   {
      rhs._ptr = nullptr;
   }

   font::~font()
   {
      if (_ptr)
         CFRelease(_ptr);
   }

   font& font::operator=(font const& rhs)
   {
      if (this != &rhs)
         _ptr = (font_impl_ptr) CFRetain(rhs._ptr);
      return *this;
   }

   font& font::operator=(font&& rhs) noexcept
   {
      if (this != &rhs)
      {
         _ptr = rhs._ptr;
         rhs._ptr = nullptr;
      }
      return *this;
   }

   font::metrics_info font::metrics() const
   {
      NSFont* font = (__bridge NSFont*) _ptr;
      return {
         float([font ascender]),
         float(-[font descender]),
         float([font leading])
      };
   }

   float font::measure_text(std::string_view str) const
   {
      NSFont* font = (__bridge NSFont*) _ptr;
      NSDictionary* attr = @{NSFontAttributeName : font};
      NSString* text = detail::ns_string(str);
      const auto textSize = [text sizeWithAttributes : attr];
      return textSize.width;
   }
}


