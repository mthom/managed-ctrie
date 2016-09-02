#ifndef VCMS_CTRIE_HPP_INCLUDED
#define VCMS_CTRIE_HPP_INCLUDED

#include <algorithm>
#include <atomic>
#include <iostream>
#include <memory>
#include <string>

#include "atomic_list.hpp"
#include "ctrie.hpp"
#include "ctrie_type_tags.hpp"
#include "impl_details.hpp"
#include "mutator.hpp"
#include "gc.hpp"
#include "ref_string.hpp"
#include "write_barrier.hpp"

using namespace kl_ctrie;
using namespace otf_gc;

std::unique_ptr<gc> gc::collector;

template <typename T>
struct local_hash
{
  using result_type = size_t;
    
  static void hash_combine(size_t& seed, char c) {
    seed ^= c + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }

  size_t operator()(const T& s) const
  {
    size_t hash = 0;
    for(auto it = s.cbegin(), end = s.cend(); it != end; ++it)
      hash_combine(hash, *it);
    return hash;
  }
};

inline std::unique_ptr<gc::registered_mutator>& mt()
{
  static thread_local std::unique_ptr<gc::registered_mutator> mt = gc::get_mutator();
  return mt;
}

template <typename T>
class otf_ctrie_allocator
{
public:
  using value_type = T;

  otf_ctrie_allocator() = default;
  
  template <class U>
  otf_ctrie_allocator(const otf_ctrie_allocator<U>&) {}

  inline value_type* allocate(size_t n)
  {    
    void* ptr = mt()->allocate(n * sizeof(T),
			       static_cast<impl_details::underlying_header_t>(ctrie_type_info<T>::header_value),
			       ctrie_type_info<T>::num_log_ptrs);

    return reinterpret_cast<T*>(ptr);
  }

  inline void deallocate(T*, size_t) {}
};

class otf_ctrie_tracer;

template <typename T>
using otf_ctrie_write_barrier = otf_write_barrier<mt, otf_ctrie_tracer, T>;

class string_allocator
{
 public:
  using value_type = char;

  string_allocator() = default;

  template<class>
  struct rebind { typedef string_allocator other; };

  static char* shallow_copy_ref_string(char* data)
  {
    using namespace impl_details;
    
    header_t* hp = reinterpret_cast<header_t*>(data - header_size);
    auto h = hp->load(std::memory_order_relaxed);

    h = ((h >> color_bits) << color_bits) | static_cast<underlying_header_t>(mt()->mut_color().c);
    hp->store(h, std::memory_order_relaxed);

    return data;
  }
  
  value_type* allocate(std::size_t n)
  {
    using namespace impl_details;

    underlying_header_t h =
      (n << tag_bits) | static_cast<underlying_header_t>(ctrie_internal_types::SV_t);

    void* ptr = mt()->allocate(n * sizeof(value_type), h, 0);
    
    return reinterpret_cast<value_type*>(ptr);
  }

  void deallocate(value_type*, size_t) {}
};

using ctrie_string = ref_string<string_allocator>;

template <typename T>
class branch_vector_allocator<otf_ctrie_write_barrier<T>>
{
 public:
  using value_type = otf_ctrie_write_barrier<T>;

  branch_vector_allocator() = default;

  value_type* allocate(std::size_t sz)
  {
    using namespace impl_details;

    underlying_header_t h =
      (sz << tag_bits) | static_cast<underlying_header_t>(ctrie_internal_types::BV_t);
    void* ptr = mt()->allocate(sz * sizeof(value_type), h, 0);
    
    return reinterpret_cast<value_type*>(ptr);
  }

  void deallocate(value_type*, size_t) {}
};

class otf_ctrie_tracer
{  
private:    
  static list<void*> trace_inode(void* ptr)
  {    
    auto& in = *reinterpret_cast<inode<ctrie_string,
				       int,
				       local_hash<ctrie_string>,
				       otf_ctrie_allocator,
				       otf_ctrie_write_barrier>*>(ptr);
      
    auto mn = in.main.load(std::memory_order_relaxed);

    if(mn) {
      return { mn->derived_ptr() };
    } else
      return {};
  }

  static list<void*> trace_cnode(void* ptr)
  {
    using namespace impl_details;
    
    auto cn = reinterpret_cast<cnode<ctrie_string,
				     int,
				     local_hash<ctrie_string>,
				     otf_ctrie_allocator,
				     otf_ctrie_write_barrier>*>(ptr);
    
    list<void*> result;
    
    if(auto cpn = cn->prev.load(std::memory_order_relaxed))
      result.push_front(cpn->derived_ptr());

    if(cn->arr.data())
      result.push_front(reinterpret_cast<void*>(cn->arr.data()));

    for(auto& p : cn->arr)
      if(p.get())
	result.push_front(reinterpret_cast<void*>(p->derived_ptr()));
    
    return result;
  }

  static list<void*> trace_snode(void* ptr)
  {
    auto& sn = *reinterpret_cast<snode<ctrie_string,
				       int,
				       local_hash<ctrie_string>,
				       otf_ctrie_allocator,
				       otf_ctrie_write_barrier>*>(ptr);

    if(sn.k.data())
      return { reinterpret_cast<void*>(const_cast<char*>(sn.k.data())) };
    else
      return {};
  }

  static list<void*> trace_tnode(void* ptr)
  {
    auto& tn = *reinterpret_cast<tnode<ctrie_string,
				       int,
				       local_hash<ctrie_string>,
				       otf_ctrie_allocator,
				       otf_ctrie_write_barrier>*>(ptr);

    list<void*> result;
    
    if(auto tpn = tn.prev.load(std::memory_order_relaxed))
      result.push_front(tpn->derived_ptr());

    if(tn.sn)
      result.push_front(const_cast<void*>(reinterpret_cast<const void*>(tn.sn)));
    
    return result;
  }

  static list<void*> trace_lnode(void* ptr)
  {      
    using internal_pl_type = kl_ctrie::snode<ctrie_string,
					     int,
					     local_hash<ctrie_string>,
					     otf_ctrie_allocator,
					     otf_ctrie_write_barrier>*;
      
    using pl_type = plist_node<internal_pl_type>;
    
    auto& ln = *reinterpret_cast<lnode<ctrie_string,
				       int,
				       local_hash<ctrie_string>,
				       otf_ctrie_allocator,
				       otf_ctrie_write_barrier>*>(ptr);

    const auto& n = *reinterpret_cast<const std::atomic<pl_type*>*>(&ln.contents);      
    const void* v = reinterpret_cast<const void*>(n.load(std::memory_order_relaxed));

    list<void*> result;
    
    if(auto lpn = ln.prev.load(std::memory_order_relaxed))
      result.push_front(lpn->derived_ptr());

    if(v)
      result.push_front(const_cast<void*>(v));
    
    return result;    
  }

  static list<void*> trace_failure(void* ptr)
  {
    auto& fn = *reinterpret_cast<failure<ctrie_string,
					 int,
					 local_hash<ctrie_string>,
					 otf_ctrie_allocator,
					 otf_ctrie_write_barrier>*>(ptr);

    if(fn.prev.get())
      return { fn.prev->derived_ptr() };
    else
      return {};
  }

  static list<void*> trace_branch_vector(void*)
  {
    return {};
  } 

  static list<void*> trace_string(void*)
  {
    return {};
  }

  static list<void*> trace_plist_node(void* ptr)
  {
    using internal_pl_type = kl_ctrie::snode<ctrie_string,
					     int,
					     local_hash<ctrie_string>,
					     otf_ctrie_allocator,
					     otf_ctrie_write_barrier>*;
    
    using pl_type = const plist_node<internal_pl_type>;

    const auto& n  = *reinterpret_cast<pl_type*>(ptr);
    const auto& nd = static_cast<const internal_pl_type&>(n.data);

    list<void*> result;

    if(n.next)
      result.push_front(reinterpret_cast<void*>(n.next));

    if(nd)
      result.push_front(const_cast<void*>(reinterpret_cast<const void*>(nd)));
    
    return result;
  }
  
  static list<void*> trace_rdcss_desc(void* ptr)
  {    
    using rdcss_desc = rdcss_descriptor<ctrie_string,
					int,
					local_hash<ctrie_string>,
					otf_ctrie_allocator,
					otf_ctrie_write_barrier>;

    list<void*> result;

    auto rd_ptr = reinterpret_cast<rdcss_desc*>(ptr);

    if(rd_ptr->ov.get())
      result.push_front(rd_ptr->ov.get()->derived_ptr());
    
    if(rd_ptr->expected_main.get())
      result.push_front(rd_ptr->expected_main.get()->derived_ptr());

    if(rd_ptr->nv.get())
      result.push_front(rd_ptr->nv.get()->derived_ptr());
    
    return result; 
  }
  
  static list<void*> trace_nothing(void*)
  {
    return {};
  }
  
  static const std::function<list<void*>(void*)> tracer_table[];  
public:
  static size_t num_log_ptrs(impl_details::underlying_header_t h)
  {
    using namespace impl_details;
    
    static const size_t log_ptr_num_table[] = {
      1, //Inode
      1, //Cnode
      0, //Snode
      0, //Tnode
      0, //Lnode
      1, //Failure
      0, //branch
      0, //plist_node<snode>
      0, //char
      1 //rdcss_descriptor
    };
    
    return log_ptr_num_table[(h & header_tag_mask) >> color_bits];
  }

  static size_t size_of(impl_details::underlying_header_t h)
  {
    using namespace impl_details;
    using namespace kl_ctrie;
    
    static const size_t sizes_table[] = {
      sizeof(inode<ctrie_string, int, local_hash<ctrie_string>, otf_ctrie_allocator, otf_ctrie_write_barrier>), //Inode
      sizeof(cnode<ctrie_string, int, local_hash<ctrie_string>, otf_ctrie_allocator, otf_ctrie_write_barrier>), //Cnode
      sizeof(snode<ctrie_string, int, local_hash<ctrie_string>, otf_ctrie_allocator, otf_ctrie_write_barrier>), //Snode
      sizeof(tnode<ctrie_string, int, local_hash<ctrie_string>, otf_ctrie_allocator, otf_ctrie_write_barrier>), //Tnode
      sizeof(lnode<ctrie_string, int, local_hash<ctrie_string>, otf_ctrie_allocator, otf_ctrie_write_barrier>), //Lnode
      sizeof(failure<ctrie_string, int, local_hash<ctrie_string>, otf_ctrie_allocator, otf_ctrie_write_barrier>), //Failure
      0,
      0,
      sizeof(plist_node<snode<ctrie_string, int, local_hash<ctrie_string>, otf_ctrie_allocator, otf_ctrie_write_barrier>>), //plist_node<snode>      
      sizeof(rdcss_descriptor<ctrie_string, int, local_hash<ctrie_string>, otf_ctrie_allocator, otf_ctrie_write_barrier>), //rdcss_descriptor
    };

    return sizes_table[(h & header_tag_mask) >> color_bits];
  }
  
  static list<void*> get_derived_ptrs(impl_details::underlying_header_t h, void* root)
  {
    using namespace impl_details;      
    std::ptrdiff_t d = reinterpret_cast<std::ptrdiff_t>(root);
    return tracer_table[(h & header_tag_mask) >> color_bits](reinterpret_cast<void*>(d));
  }

  static void* copy_obj(impl_details::underlying_header_t h, void* root)
  {
    using namespace impl_details;
    auto type_tag = (h & header_tag_mask) >> color_bits;
    
    if(type_tag == static_cast<uint8_t>(ctrie_internal_types::BV_t)) {
      using value_type = branch<ctrie_string,
				int,
				local_hash<ctrie_string>,
				otf_ctrie_allocator,
				otf_ctrie_write_barrier>*;
      
      size_t n = h >> (color_bits + tag_bits);      
      void* buf = aligned_alloc(alignof(header_t), header_size + n * sizeof(value_type));

      new(reinterpret_cast<header_t*>(buf)) header_t(h);

      std::memcpy(reinterpret_cast<void*>(reinterpret_cast<std::ptrdiff_t>(buf) + header_size),
		  root,
		  n * sizeof(value_type));
      
      return reinterpret_cast<void*>(reinterpret_cast<std::ptrdiff_t>(buf) + header_size);
    } else if(type_tag == static_cast<uint8_t>(ctrie_internal_types::SV_t)) {
      return nullptr;
    }

    void* buf = aligned_alloc(alignof(header_t), size_of(h) + header_size);
    new(reinterpret_cast<header_t*>(buf)) header_t(h);

    std::memcpy(reinterpret_cast<void*>(reinterpret_cast<std::ptrdiff_t>(buf) + header_size),
		root,
		size_of(h));
    
    return reinterpret_cast<void*>(reinterpret_cast<std::ptrdiff_t>(buf) + header_size);
  }

  inline static void*
  copy_obj_segment(impl_details::underlying_header_t h, void* root, size_t)
  {
    return copy_obj(h, root);
  }
  
  inline static list<void*>
  derived_ptrs_of_obj_segment(impl_details::underlying_header_t h, void* root, size_t)
  {
    return get_derived_ptrs(h, root);
  }
    
  inline static impl_details::log_ptr_t*
  log_ptr(impl_details::underlying_header_t h, void* parent, size_t)
  {
    using namespace impl_details;

    size_t offset = num_log_ptrs(h);
    auto pd = reinterpret_cast<std::ptrdiff_t>(parent) - header_size - offset * log_ptr_size;
      
    return reinterpret_cast<log_ptr_t*>(pd);
  }
};

class otf_ctrie
{
public:
  class otf_ctrie_policy
  {
  private:
    template <typename T>
    static void destroy_type(void* ptr)
    {
      T* t_ptr = reinterpret_cast<T*>(ptr);
      t_ptr->~T();
    }

    static void destroy_nothing(void*) {}
    
    static void destroy_vector(void*)
    {}
    
    static const std::function<void(void*)> destructor_table[];  
  public:    
    using roots_type = void*;
    
    using mutator_type = kl_ctrie::ctrie<ctrie_string,
					 int,
					 local_hash<ctrie_string>,
					 otf_ctrie_allocator,
					 otf_ctrie_write_barrier>;

    inline static void destroy(impl_details::underlying_header_t h, impl_details::header_t* ptr)
    {
      using namespace impl_details;
      
      std::ptrdiff_t d = reinterpret_cast<std::ptrdiff_t>(ptr) + header_size;
      destructor_table[(h & header_tag_mask) >> color_bits](reinterpret_cast<void*>(d));
    }    
  };               

  inline void poll_for_sync()
  {    
    mt()->poll_for_sync();
  }

  using inst_ctrie = ctrie<ctrie_string,
			   int,
			   local_hash<ctrie_string>,
			   otf_ctrie_allocator,
			   otf_ctrie_write_barrier>;

  inst_ctrie ct;

  otf_ctrie(inst_ctrie ct_) : ct(ct_) {}
  
public:
  otf_ctrie snapshot()
  {
    return ct.snapshot();
  }
  
  list<void*> ct_callback()
  {
    using root_type = kl_ctrie::inode_or_rdcss<ctrie_string,
						   int,
						   local_hash<ctrie_string>,
						   otf_ctrie_allocator,
						   otf_ctrie_write_barrier>*;
	
    otf_ctrie_write_barrier<std::atomic<root_type>>& rt =
      *reinterpret_cast<otf_ctrie_write_barrier<std::atomic<root_type>>*>(&ct);
	
    auto item = rt.load(std::memory_order_relaxed);
    return list<void*>({ item->derived_ptr() });
  }
  
  otf_ctrie()
  {
    mt()->set_root_callback([this]() {
	return this->ct_callback();
      });
  }
  
  inline void insert(ctrie_string k, int v)
  {
    poll_for_sync();
    ct.insert(k, v);
  }

  inline const int* remove(ctrie_string k)
  {
    poll_for_sync();
    return ct.remove(k);
  }

  inline const int* lookup(ctrie_string k)
  {
    poll_for_sync();
    return ct.lookup(k);
  }
};

const std::function<list<void*>(void*)> otf_ctrie_tracer::tracer_table[] = {
  trace_inode,
  trace_cnode,
  trace_snode,
  trace_tnode,
  trace_lnode,
  trace_failure,
  trace_branch_vector,
  trace_string,
  trace_plist_node,
  trace_rdcss_desc
};

const std::function<void(void*)> otf_ctrie::otf_ctrie_policy::destructor_table[] = {
  destroy_type<inode<ctrie_string, int, local_hash<ctrie_string>, otf_ctrie_allocator, otf_ctrie_write_barrier>>,
  destroy_type<cnode<ctrie_string, int, local_hash<ctrie_string>, otf_ctrie_allocator, otf_ctrie_write_barrier>>,
  destroy_type<snode<ctrie_string, int, local_hash<ctrie_string>, otf_ctrie_allocator, otf_ctrie_write_barrier>>,
  destroy_type<tnode<ctrie_string, int, local_hash<ctrie_string>, otf_ctrie_allocator, otf_ctrie_write_barrier>>,
  destroy_type<lnode<ctrie_string, int, local_hash<ctrie_string>, otf_ctrie_allocator, otf_ctrie_write_barrier>>,
  destroy_type<failure<ctrie_string, int, local_hash<ctrie_string>, otf_ctrie_allocator, otf_ctrie_write_barrier>>,  
  destroy_vector,
  destroy_vector,
  destroy_type<plist_node<snode<ctrie_string, int, local_hash<ctrie_string>, otf_ctrie_allocator, otf_ctrie_write_barrier>>>,
  destroy_type<rdcss_descriptor<ctrie_string, int, local_hash<ctrie_string>, otf_ctrie_allocator, otf_ctrie_write_barrier>>
};
#endif
