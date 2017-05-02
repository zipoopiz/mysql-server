/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MAP_HELPERS_INCLUDED
#define MAP_HELPERS_INCLUDED

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <unordered_map>
#include <unordered_set>

#include "m_ctype.h"
#include "my_inttypes.h"
#include "sql/malloc_allocator.h"
#include "sql/memroot_allocator.h"
#include "template_utils.h"

/**
  Some useful helpers for associative arrays with MySQL-specific semantics.
*/

/**
  For unordered_map<Key, T*> or unordered_map<Key, unique_ptr<T>>, does
  find() and returns nullptr if the element was not found.

  It is not possible to distinguish between "not found" and "found, but
  contained nullptr" in this case. Thus, you should normally prefer checking
  against container.end() yourself.
*/
template<class Container, class Key>
static inline auto find_or_nullptr(const Container &container, const Key &key)
  -> typename std::enable_if<
       std::is_pointer<typename Container::value_type::second_type>::value,
       typename Container::value_type::second_type>::type
{
  const auto it= container.find(key);
  if (it == container.end())
    return nullptr;
  else
    return it->second;
}

template<class Container, class Key>
static inline auto find_or_nullptr(const Container &container, const Key &key)
  -> typename std::enable_if<
       std::is_pointer<typename Container::value_type::second_type::pointer>::value,
       typename Container::value_type::second_type::pointer>::type
{
  const auto it= container.find(key);
  if (it == container.end())
    return nullptr;
  else
    return it->second.get();
}

/**
  std::unique_ptr, but with a custom delete function.
  Normally, it is more efficient to have a deleter class instead,
  but this allows you to have a unique_ptr to a forward-declared class,
  so it keeps include dependencies down somewhat.
*/
template<class T>
using unique_ptr_with_deleter = std::unique_ptr<T, void(*)(T*)>;

/** A Hasher that hashes std::strings according to a MySQL collation. */
class Collation_hasher
{
public:
  explicit Collation_hasher(CHARSET_INFO *cs_arg) : cs(cs_arg) {}

  size_t operator() (const std::string &s) const
  {
    ulong nr1= 1, nr2= 4;
    cs->coll->hash_sort(
      cs, pointer_cast<const uchar *>(s.data()), s.size(), &nr1, &nr2);
    return nr1;
  }

private:
  CHARSET_INFO *cs;
};

/** A KeyEqual that compares std::strings according to a MySQL collation. */
class Collation_key_equal
{
public:
  explicit Collation_key_equal(CHARSET_INFO *cs_arg) : cs(cs_arg) {}

  size_t operator() (const std::string &a, const std::string &b) const
  {
    return cs->coll->strnncollsp(
      cs,
      pointer_cast<const uchar *>(a.data()), a.size(),
      pointer_cast<const uchar *>(b.data()), b.size()) == 0;
  }

private:
  CHARSET_INFO *cs;
};

/**
  std::unordered_map, but with my_malloc, so that you can track the memory
  used using PSI memory keys.
*/
template<class Key, class Value,
         class Hash = std::hash<Key>,
         class KeyEqual = std::equal_to<Key>>
class malloc_unordered_map
  : public std::unordered_map<Key, Value, Hash, KeyEqual,
                              Malloc_allocator<std::pair<const Key, Value>>>
{
public:
  /*
    In theory, we should be allowed to send in the allocator only, but GCC 4.8
    is missing several unordered_map constructors, so let's give in everything.
  */
  malloc_unordered_map(PSI_memory_key psi_key)
    : std::unordered_map<Key, Value, Hash, KeyEqual,
                         Malloc_allocator<std::pair<const Key, Value>>>
        (/*bucket_count=*/ 10, Hash(), KeyEqual(),
         Malloc_allocator<>(psi_key)) {}
};

/**
  std::unordered_map, but with my_malloc and collation-aware comparison.
*/
template<class Key, class Value>
class collation_unordered_map
  : public std::unordered_map<Key, Value, Collation_hasher, Collation_key_equal,
                              Malloc_allocator<std::pair<const Key, Value>>>
{
public:
  collation_unordered_map(CHARSET_INFO *cs, PSI_memory_key psi_key)
    : std::unordered_map<Key, Value, Collation_hasher, Collation_key_equal,
                         Malloc_allocator<std::pair<const Key, Value>>>
        (/*bucket_count=*/ 10, Collation_hasher(cs), Collation_key_equal(cs),
         Malloc_allocator<>(psi_key)) {}
};

/** std::unordered_set, but allocated on a MEM_ROOT.  */
template<class Key,
         class Hash = std::hash<Key>,
         class KeyEqual = std::equal_to<Key>>
class memroot_unordered_set
  : public std::unordered_set<Key, Hash, KeyEqual, Memroot_allocator<Key>>
{
public:
  /*
    In theory, we should be allowed to send in the allocator only, but GCC 4.8
    is missing several unordered_set constructors, so let's give in everything.
  */
  memroot_unordered_set(MEM_ROOT *mem_root)
    : std::unordered_set<Key, Hash, KeyEqual, Memroot_allocator<Key>>
        (/*bucket_count=*/ 10, Hash(), KeyEqual(),
         Memroot_allocator<Key>(mem_root)) {}
};
#endif  // MAP_HELPERS_INCLUDED
