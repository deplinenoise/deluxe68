#pragma once

#include <string.h>
#include <assert.h>
#include <algorithm>
#include <functional>

class StringFragment
{
  const char* m_Ptr = nullptr;
  size_t      m_Len = 0;

public:
  StringFragment() = default;

  StringFragment(const StringFragment&) = default;

  StringFragment& operator=(const StringFragment&) = default;

  StringFragment(const char* str)
    : m_Ptr(str)
    , m_Len(strlen(str))
  {}

  constexpr StringFragment(const char* str, size_t len)
    : m_Ptr(str)
    , m_Len(len)
  {}

  StringFragment skip(size_t count) const
  {
    size_t mm = std::min(count, m_Len);
    return StringFragment(m_Ptr + mm, m_Len - mm);
  }

  // Int is convenient for %.* style printfs
  int length() const { return static_cast<int>(m_Len); }

  const char* ptr() const { return m_Ptr; }

  const char* begin() const { return ptr(); }
  const char* end() const { return ptr() + length(); }

  StringFragment slice(size_t count)
  {
    size_t mm = std::min(count, m_Len);

    StringFragment result(m_Ptr, mm);

    m_Ptr += mm;
    m_Len -= mm;

    return result;
  }

  char operator[](int index) const
  {
    assert(size_t(index) < m_Len);
    return m_Ptr[index];
  }

  explicit operator bool() const
  {
    return m_Len > 0;
  }
};


inline bool operator==(const StringFragment& lhs, const StringFragment& rhs)
{
  return
    lhs.length() == rhs.length() && 
    0 == memcmp(lhs.ptr(), rhs.ptr(), lhs.length());
}

inline bool operator!=(const StringFragment& lhs, const StringFragment& rhs)
{
  return !(lhs == rhs);
}

namespace std
{
  template <>
  struct hash<StringFragment>
  {
    size_t operator()(const StringFragment& f) const
    {
      size_t hash = 5381;

      for (int i = 0, len = f.length(); i < len; ++i)
      {
        hash = ((hash << 5) + hash) + static_cast<size_t>(f[i]); /* hash * 33 + c */
      }

      return hash;
    }
  };
}
