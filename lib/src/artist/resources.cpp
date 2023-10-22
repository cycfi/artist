/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <algorithm>
#include <artist/resources.hpp>
#include <iostream>
#include <mutex>
#include <vector>

namespace cycfi::artist
{
   std::pair<std::vector<fs::path>&, std::mutex&>
   get_resource_paths()
   {
      static std::vector<fs::path> resource_paths;
      static std::mutex resource_paths_mutex;
      return {resource_paths, resource_paths_mutex};
   }

   void add_search_path(fs::path const& path, bool search_first)
   {
      auto [resource_paths, resource_paths_mutex] = get_resource_paths();
      std::lock_guard<std::mutex> guard(resource_paths_mutex);
      if (search_first)
         resource_paths.insert(resource_paths.begin(), path);
      else
         resource_paths.push_back(path);
   }

   namespace
   {
      struct resource_setter
      {
         resource_setter()
         {
            init_paths();
         }
      };
   }

   fs::path find_file(fs::path const& file)
   {
      static resource_setter init_resources;

      if (file.is_absolute())
      {
         if (fs::exists(file))
            return file;
         return fs::path{}; // Return an empty path if the file does not exist
      }

      auto [resource_paths, resource_paths_mutex] = get_resource_paths();
      std::lock_guard<std::mutex> guard(resource_paths_mutex);

      auto it = std::find_if(resource_paths.begin(), resource_paths.end(), [&file](const fs::path &path)
                             { return fs::exists(path / file); });

      return (it != resource_paths.end()) ? (*it / file) : fs::path{};
   }

   fs::path find_directory(fs::path const& dir)
   {
      static resource_setter init_resources;

      if (dir.is_absolute())
      {
         if (fs::is_directory(dir))
            return dir;
         return fs::path{}; // Return an empty path if the directory does not exist
      }

      auto [resource_paths, resource_paths_mutex] = get_resource_paths();
      std::lock_guard<std::mutex> guard(resource_paths_mutex);

      auto it = std::find_if(resource_paths.begin(), resource_paths.end(), [&dir](const fs::path &path)
                             { return fs::is_directory(path / dir); });

      return (it != resource_paths.end()) ? (*it / dir) : fs::path{};
   }

}
