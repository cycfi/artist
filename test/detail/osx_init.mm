/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <Quartz/Quartz.h>
#include <dlfcn.h>
#include <infra/assert.hpp>
#include <artist/resources.hpp>

///////////////////////////////////////////////////////////////////////////////
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
}

struct resource_setter
{
   resource_setter();
};

resource_setter::resource_setter()
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

