#ifndef REF_STRING_HPP_INCLUDED
#define REF_STRING_HPP_INCLUDED

#include <cstring>
#include <ostream>

template <class Alloc>
class ref_string;

template <class Alloc>
std::ostream& operator<<(std::ostream&, const ref_string<Alloc>&);

template <class Alloc = std::allocator<char>>
class ref_string
{
private:
  size_t n;
  char* str;

  struct iterator {
    char* i;

    inline char& operator*() {
      return *i;
    }
    
    inline iterator& operator++() {
      ++i;
      return *this;
    }

    inline iterator operator++(int) {
      return iterator { i++ };
    }

    inline iterator& operator--() {
      return --i;
    }

    inline iterator& operator--(int) {
      return iterator { i-- };
    }
    
    inline bool operator==(const iterator& it) const {
      return i == it.i;
    }

    inline bool operator!=(const iterator& it) const {
      return i != it.i;
    }
  };
  
  struct const_iterator {
    const char* i;

    inline const char& operator*() {
      return *i;
    }
    
    inline const_iterator& operator++() {
      ++i;
      return *this;
    }

    inline const_iterator operator++(int) {
      return iterator { i++ };
    }

    inline const_iterator& operator--() {
      return iterator { --i };
    }

    inline const_iterator operator--(int) const {
      return iterator { i-- };
    }
    
    inline bool operator==(const const_iterator& it) const {
      return i == it.i;
    }

    inline bool operator!=(const const_iterator& it) const {
      return i != it.i;
    }
  };
  
  friend std::ostream& operator<< <>(std::ostream&, const ref_string<Alloc>&);  
public:
  ref_string(const ref_string& ss)
    : n(ss.n), str(Alloc::shallow_copy_ref_string(ss.str))
  {
  }
  
  ref_string(size_t n_, char c)
    : n(n_), str(reinterpret_cast<char*>(Alloc().allocate(n + 1)))
  {
    std::fill(str, str + n, c);
    str[n] = '\0';
  }

  ref_string(const char* s)
    : n(std::strlen(s)), str(reinterpret_cast<char*>(Alloc().allocate(n + 1)))
  {
    std::strcpy(str, s);
  }

  inline const_iterator cbegin() const {
    return const_iterator { str };
  }
  
  inline const_iterator cend() const {
    return const_iterator { str + n };
  }
  
  inline iterator begin()
  {
    return iterator { str };
  }
  
  inline iterator end()
  {
    return iterator { str + n };
  }
  
  inline size_t size() const
  {
    return n;
  }
  
  inline char& operator[](size_t i)
  {
    return str[i];
  }

  inline bool operator==(const ref_string& ss) const
  {
    return n == ss.n && !std::strcmp(str, ss.str);
  }
  
  inline bool operator==(const char* str_) const
  {
    return !std::strcmp(str, str_);
  }

  inline ref_string& operator=(const char* str_)
  {
    n = std::strlen(str_);    
    str = Alloc().allocate(n + 1);

    std::strcpy(str, str_);

    return *this;
  }
  
  inline ref_string& operator=(const ref_string& ss)
  {
    n = ss.n;    
    str = Alloc::shallow_copy_ref_string(ss.str);

    return *this;
  }

  inline ref_string& operator=(ref_string&& ss)
  {
    n = ss.n;    
    str = Alloc::shallow_copy_ref_string(ss.str);

    ss.n = 0; ss.str = nullptr;

    return *this;
  }
  
  inline const char* data() const
  {
    return str;
  }  
};

template <class Alloc>
std::ostream& operator<<(std::ostream& os, const ref_string<Alloc>& ss)
{
  os << ss.str;
  return os;
}
#endif
