#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>

#include <unordered_map>
#include <vector>
#include <string>

#include "tokenizer.h"

class Deluxe68
{
  static constexpr size_t kLineMax = 4096;

  FILE*       m_Input;
  const char* m_Filename;
  int         m_LineNumber = 0;
  int         m_ErrorCount = 0;
  bool        m_InProc = false;

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
    int     m_AllocatedLine = 0;
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
  void proc(Tokenizer& tokenizer);
  void endProc(Tokenizer& tokenizer);
  void reserve(Tokenizer& tokenizer);
  void unreserve(Tokenizer& tokenizer);

  void killAll();

  bool expect(Tokenizer& t, TokenType type, Token* out = nullptr);
  bool accept(Tokenizer& t, TokenType type);
};

Deluxe68::Deluxe68(const char* fn)
  : m_Input(fopen(fn, "r"))
  , m_Filename(fn)
{
  killAll();

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

  Tokenizer tokenizer(p + 1);
  Token t = tokenizer.next();

  switch (t.m_Type)
  {
    case TokenType::kAreg:
    case TokenType::kDreg:
      allocRegs(tokenizer, t.m_Type);
      break;

    case TokenType::kKill:
      killRegs(tokenizer);
      break;

    case TokenType::kProc:
      proc(tokenizer);
      break;

    case TokenType::kEndProc:
      endProc(tokenizer);
      break;

    case TokenType::kReserve:
      reserve(tokenizer);
      break;

    case TokenType::kUnreserve:
      unreserve(tokenizer);
      break;

    default:
      error("unsupported syntax: %s\n", tokenTypeName(t.m_Type));
      return;
  }

  ++p;
}

void Deluxe68::allocRegs(Tokenizer& tokenizer, TokenType regType)
{
  bool first = true;

  for (;;)
  {
    if (accept(tokenizer, TokenType::kEndOfLine))
      return;

    if (!first && !expect(tokenizer, TokenType::kComma))
      return;

    first = false;

    Token ident;
    if (!expect(tokenizer, TokenType::kIdentifier, &ident))
      return;

    std::string name(ident.m_Start, ident.m_End);

    if (m_LiveRegs.find(name) != m_LiveRegs.end())
    {
      error("register name already in use: '%s'\n", name.c_str());
      continue;
    }

    int index = -1;
    const bool isAddr = regType == TokenType::kAreg ? true : false;
    RegState* s = isAddr ? m_AddrRegs : m_DataRegs;

    if (accept(tokenizer, TokenType::kLeftParen))
    {
      Token reg;
      if (!expect(tokenizer, TokenType::kRegisterName, &reg))
        return;
      if (!expect(tokenizer, TokenType::kRightParen))
        return;

      // Make sure the name checks out.
      if (isAddr && 'a' != reg.m_Start[0])
      {
        error("expected address register");
        return;
      }
      else if (!isAddr && 'd' != reg.m_Start[0])
      {
        error("expected data register");
        return;
      }

      index = int(reg.m_Start[1]) - '0';

      if (s[index] != kFree)
      {
        error("register %.*s not free here\n", 2, reg.m_Start);
        continue;
      }
    }
    else
    {
      // Grab the first free register.
      for (int i = 0; i < 8; ++i)
      {
        if (s[i] == kFree)
        {
          s[i] = kAllocated;
          index = i;
          break;
        }
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
    a.m_AllocatedLine = m_LineNumber;
    m_LiveRegs.insert(std::make_pair(name, a));
 }
}

void Deluxe68::killRegs(Tokenizer& tokenizer)
{
  bool first = true;
  for (;;)
  {
    if (accept(tokenizer, TokenType::kEndOfLine))
      return;

    if (!first && !expect(tokenizer, TokenType::kComma))
      return;

    first = false;

    Token ident;
    if (!expect(tokenizer, TokenType::kIdentifier, &ident))
      return;

    std::string name(ident.m_Start, ident.m_End);

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
  }
}

void Deluxe68::proc(Tokenizer& tokenizer)
{
  if (m_InProc)
  {
    error("already inside a procedure definition\n");
    Tokenizer subt("");
    endProc(subt);
  }

  m_InProc = true;
}

void Deluxe68::endProc(Tokenizer& tokenizer)
{
  killAll();
  m_InProc = false;
}

void Deluxe68::reserve(Tokenizer& tokenizer)
{
  do
  {
    Token reg;
    if (!expect(tokenizer, TokenType::kRegisterName, &reg))
      return;

    const bool isAddr = reg.m_Start[0] == 'a';
    RegState* s = isAddr ? m_AddrRegs : m_DataRegs;
    int index = int(reg.m_Start[1]) - '0';

    if (s[index] != kFree)
    {
      error("register %.*s not free here\n", 2, reg.m_Start);
      continue;
    }

    s[index] = kReserved;

  } while (accept(tokenizer, TokenType::kComma));
}

void Deluxe68::unreserve(Tokenizer& tokenizer)
{
  do
  {
    Token reg;
    if (!expect(tokenizer, TokenType::kRegisterName, &reg))
      return;

    const bool isAddr = reg.m_Start[0] == 'a';
    RegState* s = isAddr ? m_AddrRegs : m_DataRegs;
    int index = int(reg.m_Start[1]) - '0';

    if (s[index] != kReserved)
    {
      error("register %.*s not reserved here\n", 2, reg.m_Start);
      continue;
    }

    s[index] = kFree;

  } while (accept(tokenizer, TokenType::kComma));
}

void Deluxe68::killAll()
{
  m_LiveRegs.clear();

  for (int i = 0; i < 8; ++i)
  {
    m_DataRegs[i] = kFree;
  }

  for (int i = 0; i < 7; ++i)
  {
    m_AddrRegs[i] = kFree;
  }

  m_AddrRegs[7] = kReserved;
}

void Deluxe68::bufferLine(const char* line)
{
  size_t len = strlen(line);
  m_Output.insert(m_Output.end(), line, line + len);
}

bool Deluxe68::expect(Tokenizer& tokenizer, TokenType type, Token* out)
{
  Token t = tokenizer.peek();
  if (t.m_Type == type)
  {
    if (out)
    {
      *out = tokenizer.next();
    }
    else
    {
      tokenizer.next();
    }

    return true;
  }
  else
  {
    error("expected %s, got %s\n", tokenTypeName(type), tokenTypeName(t.m_Type));
    return false;
  }
}

bool Deluxe68::accept(Tokenizer& tokenizer, TokenType type)
{
  Token t = tokenizer.peek();
  if (t.m_Type == type)
  {
    tokenizer.next();
    return true;
  }
  else
  {
    return false;
  }
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
