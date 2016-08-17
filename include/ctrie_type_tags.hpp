#ifndef CTRIE_TYPE_TAGS_HPP_INCLUDED
#define CTRIE_TYPE_TAGS_HPP_INCLUDED

#include "ctrie.hpp"
#include "plist.hpp"

using namespace kl_ctrie;

enum class ctrie_internal_types : uint8_t
{
  Inode_t = 0x00, // inode.
  Cnode_t, // cnode.
  Snode_t, // snode.
  Tnode_t, // tombstone node.
  Lnode_t, // list node.
  Fnode_t, // failure node
  BV_t, // ptr to branch vector area behind barrier.
  SV_t, // ptr to string vector area.
  Plnode_t, // ptr to plist node.
  Rdnode_t, // ptr to rdcss_descriptor.
};

template <class>
struct ctrie_type_info
{
  static const ctrie_internal_types header_value;
  static const size_t num_log_ptrs;
};
  
template <typename K,
	  typename V,
	  class Hash,
	  template <class> class Alloc,
	  template <class> class Barrier>
struct ctrie_type_info<inode<K, V, Hash, Alloc, Barrier>>
{
  static const ctrie_internal_types header_value = ctrie_internal_types::Inode_t;
  static const size_t num_log_ptrs = 1;
};

template <typename K,
	  typename V,
	  class Hash,
	  template <class> class Alloc,
	  template <class> class Barrier>
struct ctrie_type_info<cnode<K, V, Hash, Alloc, Barrier>>
{
  static const ctrie_internal_types header_value = ctrie_internal_types::Cnode_t;
  static const size_t num_log_ptrs = 1;
};

template <typename K,
	  typename V,
	  class Hash,
	  template <class> class Alloc,
	  template <class> class Barrier>
struct ctrie_type_info<snode<K, V, Hash, Alloc, Barrier>>
{
  static const ctrie_internal_types header_value = ctrie_internal_types::Snode_t;
  static const size_t num_log_ptrs = 0;
};

template <typename K,
	  typename V,
	  class Hash,
	  template <class> class Alloc,
	  template <class> class Barrier>
struct ctrie_type_info<tnode<K, V, Hash, Alloc, Barrier>>
{
  static const ctrie_internal_types header_value = ctrie_internal_types::Tnode_t;
  static const size_t num_log_ptrs = 0;
};

template <typename K,
	  typename V,
	  class Hash,
	  template <class> class Alloc,
	  template <class> class Barrier>
struct ctrie_type_info<lnode<K, V, Hash, Alloc, Barrier>>
{
  static const ctrie_internal_types header_value = ctrie_internal_types::Lnode_t;
  static const size_t num_log_ptrs = 0;
};

template <typename K,
	  typename V,
	  class Hash,
	  template <class> class Alloc,
	  template <class> class Barrier>
struct ctrie_type_info<failure<K, V, Hash, Alloc, Barrier>>
{
  static const ctrie_internal_types header_value = ctrie_internal_types::Fnode_t;
  static const size_t num_log_ptrs = 1;
};

template <typename K,
	  typename V,
	  class Hash,
	  template <class> class Alloc,
	  template <class> class Barrier>
struct ctrie_type_info<Barrier<branch<K, V, Hash, Alloc, Barrier>*>>
{
  static const ctrie_internal_types header_value = ctrie_internal_types::BV_t;
  static const size_t num_log_ptrs = 0;
};

template <typename K,
	  typename V,
	  class Hash,
	  template <class> class Alloc,
	  template <class> class Barrier>
struct ctrie_type_info<plist_node<snode<K, V, Hash, Alloc, Barrier>>>
{
  static const ctrie_internal_types header_value = ctrie_internal_types::Plnode_t;
  static const size_t num_log_ptrs = 0;
};

template <>
struct ctrie_type_info<char>
{
  static const ctrie_internal_types header_value = ctrie_internal_types::SV_t;
  static const size_t num_log_ptrs = 0;
};

template <typename K,
	  typename V,
	  class Hash,
	  template <class> class Alloc,
	  template <class> class Barrier>
struct ctrie_type_info<rdcss_descriptor<K, V, Hash, Alloc, Barrier>>
{
  static const ctrie_internal_types header_value = ctrie_internal_types::Rdnode_t;
  static const size_t num_log_ptrs = 1;
};

#endif
