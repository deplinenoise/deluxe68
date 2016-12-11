#pragma once

#include <string.h>
#include <assert.h>
#include <algorithm>
#include <functional>

class StringFragment
{
  const char* m_Ptr = nullptr;
  int32_t     m_Len = 0;  // 32 bits to be able to stick other things after it more effectively

public:
  StringFragment() = default;

  StringFragment(const StringFragment&) = default;

  StringFragment& operator=(const StringFragment&) = default;

  StringFragment(const char* str)
    : m_Ptr(str)
    , m_Len(static_cast<int32_t>(strlen(str)))
  {}

  StringFragment(const char* str, size_t len)
    : m_Ptr(str)
    , m_Len(static_cast<int32_t>(len))
  {}

  StringFragment skip(size_t count) const
  {
    int32_t mm = std::min(static_cast<int32_t>(count), m_Len);
    return StringFragment(m_Ptr + mm, m_Len - mm);
  }

  int length() const { return m_Len; }

  const char* ptr() const { return m_Ptr; }

  StringFragment slice(size_t count)
  {
    int32_t mm = std::min(static_cast<int32_t>(count), m_Len);

    StringFragment result(m_Ptr, mm);

    m_Ptr += mm;
    m_Len -= mm;

    return result;
  }

  char operator[](int index) const
  {
    assert(index < m_Len);
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
    memcmp(lhs.ptr(), rhs.ptr(), lhs.length());
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
