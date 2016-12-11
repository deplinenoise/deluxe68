#include "tokenizer.h"

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
    "register name",
    "identifier",
    "left paren",
    "right paren",
    "proc",
    "endproc",
    "comma",
    "colon",
    "end of line",
    "unknown",
    "invalid",
  };

  return names[(int)tt];
}

Tokenizer::Tokenizer(const char* p)
  : m_Ptr(p)
{
}

Token Tokenizer::peek()
{
  if (m_Curr.m_Type == TokenType::kInvalid)
  {
    decodeNext(&m_Curr);
  }
  return m_Curr;
}

Token Tokenizer::next()
{
  Token result = peek();
  m_Curr.m_Type = TokenType::kInvalid;
  return result;
}

void Tokenizer::decodeNext(Token* t)
{
  auto set_single_char_token = [this, &t](TokenType type)
  {
    t->m_Type = type;
    t->m_Start = m_Ptr;
    t->m_End = m_Ptr + 1;
    if (type != TokenType::kEndOfLine)
      ++m_Ptr;
  };

  m_Ptr = skipWhitespace(m_Ptr);

  switch (m_Ptr[0])
  {
    case '\0': set_single_char_token(TokenType::kEndOfLine); return;
    case '(': set_single_char_token(TokenType::kLeftParen); return;
    case ')': set_single_char_token(TokenType::kRightParen); return;
    case ',': set_single_char_token(TokenType::kComma); return;
    case ':': set_single_char_token(TokenType::kColon); return;
    default: break;
  }

  const char* end = m_Ptr;
  while (isalnum(*end) || '_' == *end)
  {
    ++end;
  }

  if (end == m_Ptr)
  {
    return set_single_char_token(TokenType::kUnknown);
  }

  size_t len = end - m_Ptr;

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
  };

  for (size_t i = 0; i < sizeof(keywords)/sizeof(keywords[0]); ++i)
  {
    if (keywords[i].len != len)
      continue;
    if (0 != memcmp(keywords[i].text, m_Ptr, len))
      continue;

    t->m_Type = keywords[i].type;
    t->m_Start = m_Ptr;
    t->m_End = end;
    m_Ptr = end;
    return;
  }

  if (len == 2)
  {
    if (m_Ptr[0] == 'a' || m_Ptr[0] == 'd')
    {
      if (m_Ptr[1] >= '0' && m_Ptr[1] <= '7')
      {
        t->m_Type = TokenType::kRegisterName;
        t->m_Start = m_Ptr;
        t->m_End = end;
        m_Ptr = end;
        return;
      }
    }
  }

  t->m_Type = TokenType::kIdentifier;
  t->m_Start = m_Ptr;
  t->m_End = end;
  m_Ptr = end;
}

const char* skipWhitespace(const char* p)
{
  while (isspace(*p))
  {
    ++p;
  }
  return p;
}

