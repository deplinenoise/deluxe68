#include "tokenizer.h"
#include "registers.h"

#include <ctype.h>
#include <string.h>

const char* tokenTypeName(TokenType tt)
{
  static const char* names[] =
  {
    "areg",
    "dreg",
    "kill",
    "reserve",
    "unreserve",
    "registername",
    "identifier",
    "leftparen",
    "rightparen",
    "proc",
    "endproc",
    "comma",
    "colon",
    "endofline",
    "spill",
    "restore",
    "unknown",
    "invalid"
  };
  static_assert(sizeof(names)/sizeof(names[0]) == int(TokenType::kCount), "bad array count");

  return names[(int)tt];
}

Tokenizer::Tokenizer(StringFragment p)
  : m_Remain(p)
{
}

Token Tokenizer::peek()
{
  if (m_Curr.m_Type == TokenType::kInvalid)
  {
    m_Curr = decodeNext();
  }
  return m_Curr;
}

Token Tokenizer::next()
{
  Token result = peek();
  m_Curr.m_Type = TokenType::kInvalid;
  return result;
}

Token Tokenizer::decodeNext()
{
  m_Remain = skipWhitespace(m_Remain);

  if (!m_Remain)
  {
    return Token(TokenType::kEndOfLine, StringFragment());
  }

  switch (m_Remain[0])
  {
    case '(': return Token(TokenType::kLeftParen, m_Remain.slice(1));
    case ')': return Token(TokenType::kRightParen, m_Remain.slice(1));
    case ',': return Token(TokenType::kComma, m_Remain.slice(1));
    case ':': return Token(TokenType::kColon, m_Remain.slice(1));
    default: break;
  }

  const char* beg = m_Remain.ptr();
  const char* end = beg;
  for (int i = 0, max = m_Remain.length(); i < max; ++i)
  {
    char ch = *end;
    if (!isalnum(ch) && '_' != ch)
      break;
  }

  if (end == beg)
  {
    return Token(TokenType::kUnknown, StringFragment());
  }

  size_t len = end - beg;

  static struct Keyword {
    size_t len;
    const char text[10];
    TokenType type;
  } keywords[] = {
    { 4, "dreg",      TokenType::kDreg },
    { 4, "areg",      TokenType::kAreg },
    { 4, "kill",      TokenType::kKill },
    { 7, "reserve",   TokenType::kReserve },
    { 9, "unreserve", TokenType::kUnreserve },
    { 4, "proc",      TokenType::kProc },
    { 7, "endproc",   TokenType::kEndProc },
    { 5, "spill",     TokenType::kSpill },
    { 7, "restore",   TokenType::kRestore },
  };

  for (size_t i = 0; i < sizeof(keywords)/sizeof(keywords[0]); ++i)
  {
    if (keywords[i].len != len)
      continue;
    if (0 != memcmp(keywords[i].text, beg, len))
      continue;

    return Token(keywords[i].type, m_Remain.slice(len));
  }

  if (len == 2)
  {
    if (beg[0] == 'a' || beg[0] == 'd')
    {
      if (beg[1] >= '0' && beg[1] <= '7')
      {
        int index = beg[0] == 'a' ? kAddressBase : kDataBase;
        index += beg[1] - '0';
        return Token(TokenType::kRegister, index);
      }
    }
  }

  return Token(TokenType::kIdentifier, m_Remain.slice(len));
}

StringFragment skipWhitespace(StringFragment input)
{
  for (int i = 0, count = input.length(); i < count; ++i)
  {
    if (!isspace(input[i]))
    {
      return input.skip(i);
    }
  }
  return StringFragment();
}

