/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Unit test for the resource search path: add_search_path, find_file and
   find_directory. Asserts the claims of docs/.../foundation/resources.adoc.
   Non-graphical: links lib/src/artist/resources.cpp directly and provides
   the host's init_paths() hook itself. Works on a scratch directory tree
   under the system temp directory, removed on exit.
=============================================================================*/
#include <artist/resources.hpp>
#include <fstream>
#include <iostream>
#include <string>

using namespace cycfi::artist;
namespace fs = cycfi::fs;

static int failures = 0;
#define CHECK(...) do { if (!(__VA_ARGS__)) { \
   std::cerr << "FAIL " << __LINE__ << ": " #__VA_ARGS__ "\n"; \
   ++failures; } } while (0)

// The host hook. The page: the library calls this itself the first time
// find_file or find_directory runs. Count the calls; add a sentinel so a
// repeat is visible in the list as well as in the count.
static int init_calls = 0;
static fs::path sentinel;

namespace cycfi::artist
{
   void init_paths()
   {
      ++init_calls;
      add_search_path(sentinel);
   }
}

static void touch(fs::path const& p)
{
   std::ofstream{p.string()} << "x";
}

int main()
{
   auto root = fs::temp_directory_path() / "artist_resources_test";
   fs::remove_all(root);
   fs::create_directories(root / "a");
   fs::create_directories(root / "b" / "sub");
   fs::create_directories(root / "c");
   fs::create_directories(root / "s");
   touch(root / "a" / "x.txt");
   touch(root / "b" / "x.txt");
   touch(root / "b" / "y.txt");
   touch(root / "c" / "x.txt");
   sentinel = root / "s";

   // Nothing has searched yet, so the hook has not run.
   CHECK(init_calls == 0);

   // Append order is search order: a before b.
   add_search_path(root / "a");
   add_search_path(root / "b");
   CHECK(find_file("x.txt") == root / "a" / "x.txt");
   CHECK(find_file("y.txt") == root / "b" / "y.txt");

   // The first search ran the hook exactly once and the sentinel is on
   // the list, appended after a and b.
   CHECK(init_calls == 1);

   // search_first puts a directory at the front and it wins.
   add_search_path(root / "c", true);
   CHECK(find_file("x.txt") == root / "c" / "x.txt");

   // A miss is an empty path.
   CHECK(find_file("missing.txt").empty());

   // An absolute path bypasses the list: itself when it exists, empty
   // when it does not, whatever the list holds.
   CHECK(find_file(root / "b" / "y.txt") == root / "b" / "y.txt");
   CHECK(find_file(root / "nowhere" / "y.txt").empty());

   // find_directory: relative and absolute, strict about directories.
   CHECK(find_directory("sub") == root / "b" / "sub");
   CHECK(find_directory(root / "b" / "sub") == root / "b" / "sub");
   CHECK(find_directory("x.txt").empty());               // a file, not a dir
   CHECK(find_directory(root / "a" / "x.txt").empty());
   CHECK(find_directory("missing").empty());

   // Duplicates are not removed; adding a again is harmless for lookup
   // because the first match still wins.
   add_search_path(root / "a");
   CHECK(find_file("x.txt") == root / "c" / "x.txt");

   ///////////////////////////////////////////////////////////////////////
   // Behaviour under review. Pinned so a change is visible; not asserted
   // to be correct. See the NOTEs on the resources page.
   ///////////////////////////////////////////////////////////////////////

   // REVIEW: find_file tests with fs::exists, so a relative name that
   // matches a directory is returned. find_directory has no such leak.
   CHECK(find_file("sub") == root / "b" / "sub");
   CHECK(find_file(root / "b" / "sub") == root / "b" / "sub");

   // REVIEW: find_file and find_directory each have their own one-shot
   // initializer, so init_paths() has now run twice, once per function,
   // although the header says it is called only one time.
   CHECK(init_calls == 2);

   fs::remove_all(root);

   if (failures)
      std::cerr << failures << " check(s) failed\n";
   else
      std::cout << "resources_test: all checks passed\n";
   return failures ? 1 : 0;
}
