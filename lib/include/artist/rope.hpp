/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_ROPE_JUNE_5_2026)
#define ARTIST_ROPE_JUNE_5_2026

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

namespace cycfi::artist
{
   /**
    * \brief
    *    A persistent (immutable-node) rope: a balanced binary tree of small
    *    element chunks that supports sublinear insert, erase, and slice.
    *
    *    Nodes are reference-counted and never mutated, so copying a rope is an
    *    O(1) structurally-shared snapshot: edits to one copy never affect
    *    another. This is the property the text engine relies on for cheap
    *    undo/redo and for sharing unchanged, already-shaped sub-spans across
    *    edits.
    *
    *    Editing invariants maintained by every operation:
    *    - no leaf exceeds `rope_max_leaf` elements (load and paste chunk;
    *      single-character typing coalesces into the caret's leaf instead of
    *      spawning a leaf per keystroke);
    *    - the tree stays within a logarithmic depth bound (rebuild-on-threshold
    *      balancing).
    *
    *    The element type `T` is arbitrary: the text engine uses `char32_t` for
    *    the buffer and, later, a paragraph type for the document tree.
    */
   template <typename T>
   class rope
   {
   public:

      using value_type = T;
      using size_type = std::size_t;

                              rope() = default;
                              rope(rope const&) = default;   // O(1) snapshot
                              rope(rope&&) = default;
      rope&                   operator=(rope const&) = default;
      rope&                   operator=(rope&&) = default;

      explicit                rope(std::vector<T> chunk);
      template <typename Iter>
                              rope(Iter first, Iter last);

      size_type               size() const;
      bool                    empty() const                 { return size() == 0; }
      T                       operator[](size_type i) const;

      // Editing: insert / erase / replace a range of elements.
      template <typename Iter>
      void                    insert(size_type pos, Iter first, Iter last);
      void                    insert(size_type pos, std::vector<T> const& seq);
      void                    erase(size_type pos, size_type len);
      template <typename Iter>
      void                    replace(size_type pos, size_type len, Iter first, Iter last);

      // Reading: materialized slice, or zero-copy chunk-wise visit of [pos,pos+len).
      std::vector<T>          substr(size_type pos, size_type len) const;
      template <typename F>
      void                    for_each(size_type pos, size_type len, F f) const;
      rope                    subrope(size_type pos, size_type len) const;
      std::vector<T>          flatten() const;

      // Diagnostics / tests.
      size_type               depth() const;
      size_type               leaf_count() const;
      size_type               max_leaf_size() const;

   private:

      struct node;
      using node_ptr = std::shared_ptr<node const>;

      struct node
      {
         std::vector<T>    chunk;      // leaf payload (empty for internal nodes)
         node_ptr          left;
         node_ptr          right;
         size_type         size = 0;   // total elements in this subtree
         size_type         depth = 1;
      };

      static bool          is_leaf(node_ptr const& n)    { return !n->left; }

      static node_ptr      make_leaf(std::vector<T> chunk);
      static node_ptr      make_internal(node_ptr l, node_ptr r);
      static node_ptr      build_balanced(std::vector<T> all);
      static node_ptr      concat(node_ptr l, node_ptr r);
      static node_ptr      insert_into(node_ptr n, size_type pos, std::vector<T> const& seq);
      static std::pair<node_ptr, node_ptr>
                           split(node_ptr n, size_type pos);
      static void          collect(node_ptr const& n, std::vector<T>& out);
      static node_ptr      maybe_balance(node_ptr n);
      template <typename F>
      static void          visit(node_ptr const& n, size_type pos, size_type len, F& f);

      explicit             rope(node_ptr root) : _root(std::move(root)) {}

      node_ptr             _root;
   };

   //--------------------------------------------------------------------------
   // Implementation
   //--------------------------------------------------------------------------

   // Leaves above this many elements are split; subtrees deeper than ~this
   // factor over the ideal height trigger a rebuild. Both are perf knobs only;
   // correctness is independent of them.
   namespace detail
   {
      constexpr std::size_t rope_max_leaf = 256;
   }

   template <typename T>
   typename rope<T>::node_ptr rope<T>::make_leaf(std::vector<T> chunk)
   {
      auto n = std::make_shared<node>();
      n->size = chunk.size();
      n->depth = 1;
      n->chunk = std::move(chunk);
      return n;
   }

   template <typename T>
   typename rope<T>::node_ptr rope<T>::make_internal(node_ptr l, node_ptr r)
   {
      auto n = std::make_shared<node>();
      n->size = l->size + r->size;
      n->depth = 1 + std::max(l->depth, r->depth);
      n->left = std::move(l);
      n->right = std::move(r);
      return n;
   }

   // Build a balanced tree over `all`, chunked into <= rope_max_leaf leaves.
   // Handles every size: empty -> null, small -> one leaf, large -> balanced.
   template <typename T>
   typename rope<T>::node_ptr rope<T>::build_balanced(std::vector<T> all)
   {
      if (all.empty())
         return nullptr;

      std::vector<node_ptr> leaves;
      for (size_type i = 0; i < all.size(); i += detail::rope_max_leaf)
      {
         size_type end = std::min(i + detail::rope_max_leaf, all.size());
         leaves.push_back(make_leaf(std::vector<T>(all.begin() + i, all.begin() + end)));
      }
      std::function<node_ptr(size_type, size_type)> build =
         [&](size_type lo, size_type hi) -> node_ptr
         {
            if (hi - lo == 1)
               return leaves[lo];
            size_type mid = lo + (hi - lo) / 2;
            return make_internal(build(lo, mid), build(mid, hi));
         };
      return build(0, leaves.size());
   }

   template <typename T>
   typename rope<T>::node_ptr rope<T>::concat(node_ptr l, node_ptr r)
   {
      if (!l) return r;
      if (!r) return l;

      // Coalesce two small adjacent leaves to keep the tree shallow and avoid
      // fragmentation from many small edits.
      if (is_leaf(l) && is_leaf(r) &&
         l->chunk.size() + r->chunk.size() <= detail::rope_max_leaf)
      {
         std::vector<T> merged;
         merged.reserve(l->chunk.size() + r->chunk.size());
         merged.insert(merged.end(), l->chunk.begin(), l->chunk.end());
         merged.insert(merged.end(), r->chunk.begin(), r->chunk.end());
         return make_leaf(std::move(merged));
      }
      return maybe_balance(make_internal(std::move(l), std::move(r)));
   }

   // Insert `seq` at `pos` by descending to the leaf that contains `pos` and
   // splicing in place.  Small inserts (typing) coalesce into that leaf; an
   // overflowing leaf is re-chunked balanced.  This is the editor hot path.
   template <typename T>
   typename rope<T>::node_ptr
   rope<T>::insert_into(node_ptr n, size_type pos, std::vector<T> const& seq)
   {
      if (!n)
         return build_balanced(seq);

      if (is_leaf(n))
      {
         std::vector<T> merged;
         merged.reserve(n->chunk.size() + seq.size());
         merged.insert(merged.end(), n->chunk.begin(), n->chunk.begin() + pos);
         merged.insert(merged.end(), seq.begin(), seq.end());
         merged.insert(merged.end(), n->chunk.begin() + pos, n->chunk.end());
         if (merged.size() <= detail::rope_max_leaf)
            return make_leaf(std::move(merged));
         return build_balanced(std::move(merged));
      }

      size_type lsz = n->left->size;
      if (pos <= lsz)
         return concat(insert_into(n->left, pos, seq), n->right);
      return concat(n->left, insert_into(n->right, pos - lsz, seq));
   }

   template <typename T>
   std::pair<typename rope<T>::node_ptr, typename rope<T>::node_ptr>
   rope<T>::split(node_ptr n, size_type pos)
   {
      if (!n)
         return {nullptr, nullptr};
      if (pos == 0)
         return {nullptr, n};
      if (pos >= n->size)
         return {n, nullptr};

      if (is_leaf(n))
      {
         std::vector<T> l(n->chunk.begin(), n->chunk.begin() + pos);
         std::vector<T> r(n->chunk.begin() + pos, n->chunk.end());
         return {make_leaf(std::move(l)), make_leaf(std::move(r))};
      }

      size_type lsz = n->left->size;
      if (pos < lsz)
      {
         auto [ll, lr] = split(n->left, pos);
         return {ll, concat(lr, n->right)};
      }
      else if (pos > lsz)
      {
         auto [rl, rr] = split(n->right, pos - lsz);
         return {concat(n->left, rl), rr};
      }
      return {n->left, n->right};
   }

   template <typename T>
   void rope<T>::collect(node_ptr const& n, std::vector<T>& out)
   {
      if (!n)
         return;
      if (is_leaf(n))
         out.insert(out.end(), n->chunk.begin(), n->chunk.end());
      else
      {
         collect(n->left, out);
         collect(n->right, out);
      }
   }

   template <typename T>
   typename rope<T>::node_ptr rope<T>::maybe_balance(node_ptr n)
   {
      if (!n || n->size <= 1)
         return n;
      // Ideal height ~ log2(size); rebuild if we drift well past it.
      size_type ideal = 0;
      for (size_type s = n->size; s > 1; s >>= 1)
         ++ideal;
      if (n->depth <= 2 * ideal + 4)
         return n;

      std::vector<T> all;
      all.reserve(n->size);
      collect(n, all);
      auto res = build_balanced(std::move(all));
      return res ? res : n;
   }

   template <typename T>
   template <typename F>
   void rope<T>::visit(node_ptr const& n, size_type pos, size_type len, F& f)
   {
      if (!n || len == 0)
         return;
      if (is_leaf(n))
      {
         size_type from = std::min(pos, n->chunk.size());
         size_type to = std::min(pos + len, n->chunk.size());
         if (to > from)
            f(n->chunk.data() + from, to - from);
         return;
      }
      size_type lsz = n->left->size;
      if (pos < lsz)
      {
         size_type left_len = std::min(len, lsz - pos);
         visit(n->left, pos, left_len, f);
         if (len > left_len)
            visit(n->right, 0, len - left_len, f);
      }
      else
      {
         visit(n->right, pos - lsz, len, f);
      }
   }

   template <typename T>
   rope<T>::rope(std::vector<T> chunk)
    : _root(build_balanced(std::move(chunk)))
   {}

   template <typename T>
   template <typename Iter>
   rope<T>::rope(Iter first, Iter last)
    : _root(build_balanced(std::vector<T>(first, last)))
   {}

   template <typename T>
   typename rope<T>::size_type rope<T>::size() const
   {
      return _root ? _root->size : 0;
   }

   template <typename T>
   typename rope<T>::size_type rope<T>::depth() const
   {
      return _root ? _root->depth : 0;
   }

   template <typename T>
   typename rope<T>::size_type rope<T>::leaf_count() const
   {
      std::function<size_type(node_ptr const&)> count =
         [&](node_ptr const& n) -> size_type
         {
            if (!n) return 0;
            if (is_leaf(n)) return 1;
            return count(n->left) + count(n->right);
         };
      return count(_root);
   }

   template <typename T>
   typename rope<T>::size_type rope<T>::max_leaf_size() const
   {
      std::function<size_type(node_ptr const&)> mx =
         [&](node_ptr const& n) -> size_type
         {
            if (!n) return 0;
            if (is_leaf(n)) return n->chunk.size();
            return std::max(mx(n->left), mx(n->right));
         };
      return mx(_root);
   }

   template <typename T>
   T rope<T>::operator[](size_type i) const
   {
      node const* n = _root.get();
      while (n && n->left)
      {
         size_type lsz = n->left->size;
         if (i < lsz)
            n = n->left.get();
         else
         {
            i -= lsz;
            n = n->right.get();
         }
      }
      return n->chunk[i];
   }

   template <typename T>
   template <typename Iter>
   void rope<T>::insert(size_type pos, Iter first, Iter last)
   {
      std::vector<T> seq(first, last);
      if (seq.empty())
         return;
      if (pos > size())
         pos = size();
      _root = insert_into(_root, pos, seq);
   }

   template <typename T>
   void rope<T>::insert(size_type pos, std::vector<T> const& seq)
   {
      insert(pos, seq.begin(), seq.end());
   }

   template <typename T>
   void rope<T>::erase(size_type pos, size_type len)
   {
      if (len == 0 || pos >= size())
         return;
      len = std::min(len, size() - pos);
      auto [l, rest] = split(_root, pos);
      auto [mid, r] = split(rest, len);
      (void) mid;
      _root = concat(l, r);
   }

   template <typename T>
   template <typename Iter>
   void rope<T>::replace(size_type pos, size_type len, Iter first, Iter last)
   {
      erase(pos, len);
      insert(pos, first, last);
   }

   template <typename T>
   template <typename F>
   void rope<T>::for_each(size_type pos, size_type len, F f) const
   {
      if (pos >= size())
         return;
      len = std::min(len, size() - pos);
      visit(_root, pos, len, f);
   }

   template <typename T>
   std::vector<T> rope<T>::substr(size_type pos, size_type len) const
   {
      std::vector<T> out;
      if (pos < size())
      {
         out.reserve(std::min(len, size() - pos));
         for_each(pos, len, [&](T const* p, size_type n)
            { out.insert(out.end(), p, p + n); });
      }
      return out;
   }

   template <typename T>
   rope<T> rope<T>::subrope(size_type pos, size_type len) const
   {
      if (pos >= size())
         return rope{};
      len = std::min(len, size() - pos);
      auto [l, rest] = split(_root, pos);
      (void) l;
      auto [mid, r] = split(rest, len);
      (void) r;
      return rope{mid};
   }

   template <typename T>
   std::vector<T> rope<T>::flatten() const
   {
      std::vector<T> out;
      out.reserve(size());
      collect(_root, out);
      return out;
   }
}

#endif
