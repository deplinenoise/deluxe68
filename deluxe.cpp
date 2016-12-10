#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>

#include <unordered_map>
#include <vector>
#include <string>

enum class TokenType
{
  kAreg,
  kDreg,
  kKill,
  kReserve,
  kRegisterName,
  kIdentifier,
  kLeftParen,
  kRightParen,
  kComma,
  kColon,
  kEndOfLine,
  kUnknown
};

struct Token
{
  TokenType m_Type;
  const char* m_Start;
  const char* m_End;
};

static const char* skipWhitespace(const char* p);

class Tokenizer
{
  const char* m_Ptr;

public:
  explicit Tokenizer(const char* p) : m_Ptr(p) {}

public:
  void next(Token* t);
};

void Tokenizer::next(Token* t)
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
  while (isalnum(*end))
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
    const char text[8];
    TokenType type;
  } keywords[] = {
    { 4, "dreg", TokenType::kDreg },
    { 4, "areg", TokenType::kAreg },
    { 4, "kill", TokenType::kKill },
    { 7, "reserve", TokenType::kReserve },
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
        t->m_Type = m_Ptr[0] == 'a' ? TokenType::kAreg : TokenType::kDreg;
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

class Deluxe68
{
  static constexpr size_t kLineMax = 4096;

  FILE*       m_Input;
  const char* m_Filename;
  int         m_LineNumber = 0;
  int         m_ErrorCount = 0;

  enum RegState : uint8_t
  {
    kFree      = 0,
    kAllocated = 1,
    kReserved  = 2,
  };

  RegState m_DataRegs[8];
  RegState m_AddrRegs[8];

  struct RegAlloc
  {
    uint8_t m_IsAddr = 0;
    uint8_t m_Index = 0;
  };

  std::unordered_map<std::string, RegAlloc> m_LiveRegs;

  std::vector<char> m_Output;

  char        m_LineBuf[kLineMax];

public:
  explicit Deluxe68(const char* fn);

  ~Deluxe68();

  void error(const char *fmt, ...);

  void run();

  int errorCount() const { return m_ErrorCount; }

private:
  void parseLine();

  void bufferLine(const char* line);

  void allocRegs(Tokenizer& tokenizer, TokenType regType);
  void killRegs(Tokenizer& tokenizer);
};

Deluxe68::Deluxe68(const char* fn)
  : m_Input(fopen(fn, "r"))
  , m_Filename(fn)
{
  for (int i = 0; i < 8; ++i)
  {
    m_DataRegs[i] = kFree;
  }
  for (int i = 0; i < 7; ++i)
  {
    m_AddrRegs[i] = kFree;
  }
  m_AddrRegs[7] = kReserved;

  if (!m_Input)
  {
    error("failed to open file");
  }
}

Deluxe68::~Deluxe68()
{
  if (FILE* f = m_Input)
  {
    fclose(f);
  }
}

void Deluxe68::error(const char *fmt, ...)
{
  if (m_LineNumber)
    fprintf(stderr, "%s(%d): ", m_Filename, m_LineNumber);
  else
    fprintf(stderr, "%s: ", m_Filename);

  va_list a;
  va_start(a, fmt);
  vfprintf(stderr, fmt, a);
  va_end(a);
  ++m_ErrorCount;
}

void Deluxe68::run()
{
  if (!m_Input)
    return;

  while (fgets(m_LineBuf, kLineMax, m_Input))
  {
    ++m_LineNumber;
    parseLine();
  }
}

void Deluxe68::parseLine()
{
  const char* p = skipWhitespace(m_LineBuf);

  if (*p != '@')
  {
    bufferLine(m_LineBuf);
    return;
  }

  error("unknown directive: %s", p);

  Tokenizer tokenizer(p + 1);
  Token t;
  tokenizer.next(&t);

  switch (t.m_Type)
  {
    case TokenType::kAreg:
    case TokenType::kDreg:
      allocRegs(tokenizer, t.m_Type);
      break;

    case TokenType::kKill:
      killRegs(tokenizer);
      break;

    default:
      error("unsupported syntax: %s", t.m_Start);
      return;
  }

  ++p;
}

void Deluxe68::allocRegs(Tokenizer& tokenizer, TokenType regType)
{
  Token t;
  for (;;)
  {
    tokenizer.next(&t);

    if (TokenType::kIdentifier != t.m_Type)
    {
      error("expected identifier\n");
      return;
    }

    std::string name(t.m_Start, t.m_End);

    if (m_LiveRegs.find(name) != m_LiveRegs.end())
    {
      error("register name already in use: '%s'\n", name.c_str());
      continue;
    }

    int index = -1;
    const bool isAddr = regType == TokenType::kAreg ? true : false;
    RegState* s = isAddr ? m_AddrRegs : m_DataRegs;

    for (int i = 0; i < 8; ++i)
    {
      if (s[i] == kFree)
      {
        s[i] = kAllocated;
        index = i;
        break;
      }
    }

    if (-1 == index)
    {
      error("out of %s registers (allocating %s)\n", isAddr ? "address" : "data", name.c_str());
      continue;
    }

    RegAlloc a;
    a.m_IsAddr = isAddr ? 1 : 0;
    a.m_Index = (uint8_t) index;
    m_LiveRegs.insert(std::make_pair(name, a));

    tokenizer.next(&t);

    if (TokenType::kEndOfLine == t.m_Type)
    {
      return;
    }
    else if (TokenType::kComma != t.m_Type)
    {
      error("expected comma\n");
      return;
    }
  }
}

void Deluxe68::killRegs(Tokenizer& tokenizer)
{
  Token t;
  for (;;)
  {
    tokenizer.next(&t);

    if (TokenType::kIdentifier != t.m_Type)
    {
      error("expected identifier\n");
      return;
    }

    std::string name(t.m_Start, t.m_End);

    auto it = m_LiveRegs.find(name);

    if (it == m_LiveRegs.end())
    {
      error("register name not in use: '%s'\n", name.c_str());
      continue;
    }

    const RegAlloc a = it->second;
    RegState* s = a.m_IsAddr ? m_AddrRegs : m_DataRegs;
    s[a.m_Index] = kFree;
    m_LiveRegs.erase(it);

    tokenizer.next(&t);

    if (TokenType::kEndOfLine == t.m_Type)
    {
      return;
    }
    else if (TokenType::kComma != t.m_Type)
    {
      error("expected comma\n");
      return;
    }
  }

}

const char* skipWhitespace(const char* p)
{
  while (isspace(*p))
  {
    ++p;
  }
  return p;
}

void Deluxe68::bufferLine(const char* line)
{
  size_t len = strlen(line);
  m_Output.insert(m_Output.end(), line, line + len);
}

int main(int argc, char* argv[])
{
  if (argc < 3)
  {
    fprintf(stderr, "usage: deluxe68 <input> <output>\n");
    exit(1);
  }

  Deluxe68 d(argv[1]);

  d.run();

  return d.errorCount() ? 1 : 0;
}
