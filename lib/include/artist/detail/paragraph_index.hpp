/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_PARAGRAPH_INDEX_JUNE_5_2026)
#define ARTIST_PARAGRAPH_INDEX_JUNE_5_2026

#include <artist/rope.hpp>
#include <algorithm>
#include <cstddef>
#include <vector>

namespace cycfi::artist::detail
{
   /**
    * \brief
    *    The paragraph partition of a text buffer: the character offsets at
    *    which paragraphs begin, where paragraphs are separated by hard '\n'
    *    breaks (a trailing '\n' yields a final empty paragraph).
    *
    *    `update` maintains the partition incrementally after an edit and
    *    returns the inclusive range of paragraph indices that must be
    *    re-laid-out, so the multi-paragraph engine (text_layout) reshapes
    *    only the paragraphs an edit actually touched instead of the whole
    *    document.
    *
    *    Note: the tail of the offset table is shifted by the edit delta, so an
    *    edit is O(paragraphs) in the offset bookkeeping (plus O(changed) to
    *    rescan). The expensive per-paragraph *reshaping* is what this keeps
    *    minimal; replacing the offset table with a Fenwick/rope-of-paragraphs
    *    to make bookkeeping O(log p) is a later optimization.
    */
   class paragraph_index
   {
   public:

      using size_type = std::size_t;

      struct changed_range
      {
         size_type   first;   // first paragraph index that was re-partitioned
         size_type   last;    // last (inclusive)
      };

      paragraph_index() : _starts{0}, _total{0} {}

      void              build(rope<char32_t> const& buf);

      // Update after an edit at `pos` that removed `removed` code points and
      // inserted `inserted`. `buf` must already reflect the edit. Returns the
      // inclusive range of paragraphs that were re-partitioned.
      changed_range     update(
                           rope<char32_t> const& buf
                         , size_type pos, size_type removed, size_type inserted
                        );

      size_type         count() const            { return _starts.size(); }
      size_type         start(size_type i) const { return _starts[i]; }
      size_type         end(size_type i) const
                        {
                           return (i + 1 < _starts.size()) ? _starts[i + 1] : _total;
                        }
      size_type         index_at(size_type pos) const
                        {
                           auto it = std::upper_bound(_starts.begin(), _starts.end(), pos);
                           return size_type(it - _starts.begin()) - 1;
                        }

      std::vector<size_type> const& starts() const { return _starts; }

   private:

      // Append the paragraph starts found within buf[region_start, region_end)
      // (the '\n'-following offsets) to `out`. `out` must already contain
      // `region_start` as its seed.
      static void       scan(
                           rope<char32_t> const& buf
                         , size_type region_start, size_type region_end
                         , std::vector<size_type>& out
                        );

      std::vector<size_type>  _starts;   // _starts[0] == 0 always
      size_type               _total;    // buffer size
   };

   //--------------------------------------------------------------------------
   // Implementation
   //--------------------------------------------------------------------------

   inline void paragraph_index::scan(
      rope<char32_t> const& buf
    , size_type region_start, size_type region_end
    , std::vector<size_type>& out
   )
   {
      if (region_end <= region_start)
         return;
      size_type idx = region_start;
      buf.for_each(region_start, region_end - region_start,
         [&](char32_t const* p, size_type n)
         {
            for (size_type k = 0; k < n; ++k)
               if (p[k] == U'\n')
                  out.push_back(idx + k + 1);
            idx += n;
         });
   }

   inline void paragraph_index::build(rope<char32_t> const& buf)
   {
      _total = buf.size();
      _starts.clear();
      _starts.push_back(0);
      scan(buf, 0, _total, _starts);
   }

   inline paragraph_index::changed_range paragraph_index::update(
      rope<char32_t> const& buf
    , size_type pos, size_type removed, size_type inserted
   )
   {
      // Paragraphs spanned by the removed region, on the pre-edit partition.
      size_type p0 = index_at(pos);
      size_type pe = index_at(pos + removed);

      size_type region_start = _starts[p0];
      size_type old_region_end = (pe + 1 < _starts.size()) ? _starts[pe + 1] : _total;
      long delta = long(inserted) - long(removed);
      size_type new_region_end = size_type(long(old_region_end) + delta);

      // Re-partition just the affected region against the post-edit buffer.
      std::vector<size_type> rebuilt;
      rebuilt.push_back(region_start);
      scan(buf, region_start, new_region_end, rebuilt);

      // Paragraph `pe`'s trailing '\n' (kept by the edit) produces a boundary
      // equal to the tail's head; when a tail follows, drop it so it isn't
      // counted twice. With no tail, a trailing '\n' is the real final empty
      // paragraph and is kept.
      bool has_tail = (pe + 1) < _starts.size();
      if (has_tail && rebuilt.size() > 1 && rebuilt.back() == new_region_end)
         rebuilt.pop_back();

      // Splice: [ unchanged head | rebuilt | shifted tail ].
      std::vector<size_type> result;
      result.reserve(p0 + rebuilt.size() + (_starts.size() - (pe + 1)));
      result.insert(result.end(), _starts.begin(), _starts.begin() + p0);
      result.insert(result.end(), rebuilt.begin(), rebuilt.end());
      for (size_type i = pe + 1; i < _starts.size(); ++i)
         result.push_back(size_type(long(_starts[i]) + delta));

      _starts.swap(result);
      _total = size_type(long(_total) + delta);

      return {p0, p0 + rebuilt.size() - 1};
   }
}

#endif
