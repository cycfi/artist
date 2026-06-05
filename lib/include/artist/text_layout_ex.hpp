/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_TEXT_LAYOUT_EX_JUNE_5_2026)
#define ARTIST_TEXT_LAYOUT_EX_JUNE_5_2026

#include <artist/text_layout.hpp>
#include <artist/detail/paragraph_index.hpp>
#include <artist/rope.hpp>
#include <artist/point.hpp>
#include <algorithm>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace cycfi::artist
{
   class canvas;

   /**
    * \brief
    *    Multi-paragraph text engine: a rope-backed document split into
    *    paragraphs at hard '\n' breaks, with one paragraph layout per
    *    paragraph stacked vertically.
    *
    *    Edits are incremental: only the paragraphs an edit touches are
    *    re-laid-out (via `paragraph_index`), and the rope buffer shares
    *    unchanged structure. Templated on the per-paragraph `Layout` so the
    *    stitching can be tested with a non-graphical mock; the production
    *    instantiation is `text_layout_ex` over `artist::text_layout`.
    *
    *    `Layout` must provide: construction from `std::u32string_view`,
    *    `flow(float, bool)`, `num_lines()`, `caret_point(size_t)`,
    *    `caret_index(point)`, and (only if `draw` is used) `draw(canvas&,
    *    point, color)`.
    */
   template <typename Layout>
   class basic_text_layout_ex
   {
   public:

      using size_type = std::size_t;
      using factory = std::function<Layout(std::u32string_view)>;

      static constexpr size_type npos = size_type(-1);

                              basic_text_layout_ex(
                                 factory make
                               , double line_height
                               , std::u32string_view text
                              );

                              basic_text_layout_ex(basic_text_layout_ex&&) noexcept = default;
      basic_text_layout_ex&   operator=(basic_text_layout_ex&&) noexcept = default;

      void                    set_text(std::u32string_view text);
      std::u32string          text() const;
      size_type               size() const                { return _buffer.size(); }
      size_type               paragraph_count() const     { return _paras.size(); }

      void                    insert(size_type pos, std::u32string_view text);
      void                    erase(size_type pos, size_type len);
      void                    replace(size_type pos, size_type len, std::u32string_view text);

      void                    flow(float width, bool justify = false);
      size_type               num_lines() const;
      double                  height() const               { return num_lines() * _line_height; }

      // Diagnostics.
      size_type               paragraph_lines(size_type i) const { return _paras[i].lines; }

      point                   caret_point(size_type index) const;
      size_type               caret_index(point p) const;
      size_type               caret_index(float x, float y) const { return caret_index(point{x, y}); }

      // Word/line break queries over the whole document, delegated to the
      // paragraph that owns `index`. Paragraph boundaries (hard '\n') are
      // mandatory line breaks. The return type matches the per-paragraph
      // Layout's break classification (e.g. text_layout::break_enum).
      typename Layout::break_enum   word_break(size_type index) const;
      typename Layout::break_enum   line_break(size_type index) const;

      void                    draw(canvas& cnv, point p, color c = colors::black) const;

   private:

      struct para
      {
         Layout      layout;
         size_type   lines;
         double      y;       // top of this paragraph, relative to the document
      };

      para                    make_para(size_type i) const;
      void                    splice_rebuild(
                                 detail::paragraph_index::changed_range ch, size_type old_count
                              );
      void                    recompute_offsets(size_type from);
      size_type               para_at_y(double y) const;

      factory                 _make;
      double                  _line_height;
      float                   _width = 0;
      bool                    _justify = false;
      rope<char32_t>          _buffer;
      detail::paragraph_index _px;
      std::vector<para>       _paras;
   };

   //--------------------------------------------------------------------------
   // Implementation
   //--------------------------------------------------------------------------

   template <typename Layout>
   basic_text_layout_ex<Layout>::basic_text_layout_ex(
      factory make, double line_height, std::u32string_view text)
    : _make(std::move(make))
    , _line_height(line_height)
   {
      set_text(text);
   }

   template <typename Layout>
   typename basic_text_layout_ex<Layout>::para
   basic_text_layout_ex<Layout>::make_para(size_type i) const
   {
      // Exclude the trailing '\n' separator from the shaped text: the
      // paragraph break itself provides the line advance, so handing the '\n'
      // to the layout (which would open an extra empty line) double-counts.
      auto s = _px.start(i);
      auto e = _px.end(i);
      if (e > s && _buffer[e - 1] == U'\n')
         --e;
      auto txt = _buffer.substr(s, e - s);
      Layout layout = _make(std::u32string_view(txt.data(), txt.size()));
      if (_width > 0)
         layout.flow(_width, _justify);
      // Every paragraph occupies at least one line: an empty paragraph (e.g. a
      // bare '\n' or the empty final paragraph after a trailing newline) is
      // still a visible blank line.
      size_type lines = std::max<size_type>(1, layout.num_lines());
      return para{std::move(layout), lines, 0};
   }

   template <typename Layout>
   void basic_text_layout_ex<Layout>::set_text(std::u32string_view text)
   {
      _buffer = rope<char32_t>(text.begin(), text.end());
      _px.build(_buffer);
      _paras.clear();
      _paras.reserve(_px.count());
      for (size_type i = 0; i != _px.count(); ++i)
         _paras.push_back(make_para(i));
      recompute_offsets(0);
   }

   template <typename Layout>
   std::u32string basic_text_layout_ex<Layout>::text() const
   {
      auto v = _buffer.flatten();
      return std::u32string(v.begin(), v.end());
   }

   template <typename Layout>
   void basic_text_layout_ex<Layout>::recompute_offsets(size_type from)
   {
      double y = (from == 0)
         ? 0.0
         : _paras[from - 1].y + _paras[from - 1].lines * _line_height;
      for (size_type i = from; i != _paras.size(); ++i)
      {
         _paras[i].y = y;
         y += _paras[i].lines * _line_height;
      }
   }

   template <typename Layout>
   void basic_text_layout_ex<Layout>::splice_rebuild(
      detail::paragraph_index::changed_range ch, size_type old_count)
   {
      size_type new_count = _px.count();
      long dpar = long(new_count) - long(old_count);
      // The old paragraphs [ch.first .. pe] were replaced by the new
      // [ch.first .. ch.last]; recover pe from the paragraph-count delta.
      size_type pe = ch.last - dpar;

      // Rebuild via move-construction only (the paragraph Layout, e.g.
      // text_layout, is move-constructible but not move-assignable, so
      // vector insert/erase element shifts are unavailable).
      std::vector<para> updated;
      updated.reserve(new_count);
      for (size_type i = 0; i < ch.first; ++i)
         updated.push_back(std::move(_paras[i]));
      for (size_type i = ch.first; i <= ch.last; ++i)
         updated.push_back(make_para(i));
      for (size_type i = pe + 1; i < old_count; ++i)
         updated.push_back(std::move(_paras[i]));
      _paras = std::move(updated);
      recompute_offsets(ch.first);
   }

   template <typename Layout>
   void basic_text_layout_ex<Layout>::insert(size_type pos, std::u32string_view text)
   {
      if (text.empty())
         return;
      if (pos > size())
         pos = size();
      size_type old_count = _paras.size();
      _buffer.insert(pos, text.begin(), text.end());
      auto ch = _px.update(_buffer, pos, 0, text.size());
      splice_rebuild(ch, old_count);
   }

   template <typename Layout>
   void basic_text_layout_ex<Layout>::erase(size_type pos, size_type len)
   {
      if (len == 0 || pos >= size())
         return;
      len = std::min(len, size() - pos);
      size_type old_count = _paras.size();
      _buffer.erase(pos, len);
      auto ch = _px.update(_buffer, pos, len, 0);
      splice_rebuild(ch, old_count);
   }

   template <typename Layout>
   void basic_text_layout_ex<Layout>::replace(
      size_type pos, size_type len, std::u32string_view text)
   {
      erase(pos, len);
      insert(pos, text);
   }

   template <typename Layout>
   void basic_text_layout_ex<Layout>::flow(float width, bool justify)
   {
      _width = width;
      _justify = justify;
      for (auto& p : _paras)
      {
         p.layout.flow(width, justify);
         p.lines = std::max<size_type>(1, p.layout.num_lines());
      }
      recompute_offsets(0);
   }

   template <typename Layout>
   typename basic_text_layout_ex<Layout>::size_type
   basic_text_layout_ex<Layout>::num_lines() const
   {
      size_type n = 0;
      for (auto const& p : _paras)
         n += p.lines;
      return n;
   }

   template <typename Layout>
   point basic_text_layout_ex<Layout>::caret_point(size_type index) const
   {
      size_type pi = _px.index_at(index);
      size_type local = index - _px.start(pi);
      point lp = _paras[pi].layout.caret_point(local);
      return {lp.x, float(lp.y + _paras[pi].y)};
   }

   template <typename Layout>
   typename basic_text_layout_ex<Layout>::size_type
   basic_text_layout_ex<Layout>::para_at_y(double y) const
   {
      if (_paras.empty())
         return 0;
      // Last paragraph whose top y-offset is <= y.
      size_type lo = 0, hi = _paras.size();
      while (lo < hi)
      {
         size_type mid = lo + (hi - lo) / 2;
         if (_paras[mid].y <= y)
            lo = mid + 1;
         else
            hi = mid;
      }
      return (lo == 0) ? 0 : lo - 1;
   }

   template <typename Layout>
   typename basic_text_layout_ex<Layout>::size_type
   basic_text_layout_ex<Layout>::caret_index(point p) const
   {
      // Select the paragraph whose vertical span contains p.y (a body
      // coordinate, top-relative), then resolve the row within it.
      //
      // The per-paragraph layout's caret_index returns the first row whose top
      // is >= the query y, so to land on visual row k the query must fall in
      // ((k-1), k] * line_height -- the band just above the row, NOT the row's
      // own body. A raw body click therefore overshoots by one row. Compute the
      // visual row from the body y ourselves, then query at (row - 0.5) *
      // line_height: centred in the correct selection band, robust to float
      // rounding in the row offsets. p.x is preserved, so horizontal glyph
      // resolution is unaffected.
      size_type pi = para_at_y(p.y);
      double local_y = p.y - _paras[pi].y;
      size_type row = (local_y <= 0)? 0 : size_type(local_y / _line_height);
      if (row >= _paras[pi].lines)
         row = _paras[pi].lines - 1;
      point local{p.x, float((double(row) - 0.5) * _line_height)};
      size_type li = _paras[pi].layout.caret_index(local);
      return _px.start(pi) + li;
   }

   template <typename Layout>
   typename Layout::break_enum
   basic_text_layout_ex<Layout>::word_break(size_type index) const
   {
      size_type pi = _px.index_at(index);
      return _paras[pi].layout.word_break(index - _px.start(pi));
   }

   template <typename Layout>
   typename Layout::break_enum
   basic_text_layout_ex<Layout>::line_break(size_type index) const
   {
      // A hard line break is reported at the '\n' character's own index -- a
      // mandatory break *after* that character -- matching libunibreak and the
      // single text_layout. (The editor's word/line navigation relies on this
      // convention: it lands on the newline, then steps forward onto the next
      // line's first character.)
      if (index < _buffer.size() && _buffer[index] == U'\n')
         return Layout::must_break;
      size_type pi = _px.index_at(index);
      return _paras[pi].layout.line_break(index - _px.start(pi));
   }

   template <typename Layout>
   void basic_text_layout_ex<Layout>::draw(canvas& cnv, point p, color c) const
   {
      for (auto const& pa : _paras)
         pa.layout.draw(cnv, {p.x, float(p.y + pa.y)}, c);
   }

   //--------------------------------------------------------------------------
   // Production instantiation over artist::text_layout.
   //--------------------------------------------------------------------------
   using text_layout_ex = basic_text_layout_ex<text_layout>;

   // Build a text_layout_ex for a single font; line height is derived from the
   // font metrics, and each paragraph is shaped by an artist::text_layout.
   inline text_layout_ex make_text_layout_ex(font_descr f, std::u32string_view text)
   {
      auto m = font{f}.metrics();
      float line_height = m.ascent + m.descent + m.leading;
      return text_layout_ex(
         [f](std::u32string_view s) { return text_layout{f, s}; }
       , line_height
       , text
      );
   }
}

#endif
