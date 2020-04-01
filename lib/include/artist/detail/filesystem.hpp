/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ELEMENTS_DETAIL_FILESYSTEM_APRIL_1_2020)
#define ELEMENTS_DETAIL_FILESYSTEM_APRIL_1_2020

#if defined(__apple_build_version__) && (__apple_build_version__ <= 11000033L)
# define ARTIST_HAS_NO_STD_FILESTSTEM
#endif

#if defined(__cplusplus) && __cplusplus >= 201703L && defined(__has_include)
# if __has_include(<filesystem>) && !defined(ARTIST_HAS_NO_STD_FILESTSTEM)
#  define GHC_USE_STD_FS
#  include <filesystem>
   namespace fs = std::filesystem;
# endif
#endif

#if !defined(GHC_USE_STD_FS)
# include <ghc/filesystem.hpp>
  namespace fs = ghc::filesystem;
#endif

#endif
