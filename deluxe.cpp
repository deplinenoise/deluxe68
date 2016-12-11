#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>

#include <unordered_map>
#include <vector>

#include "tokenizer.h"
#include "registers.h"
#include "stringfragment.h"

enum class OutputKind
{
  kStringLiteral,
  kNamedRegister,
  kSpill,
  kRestore,
  kProcHeader,
  kProcFooter
};

struct ProcedureDef
{
  uint32_t m_UsedRegs = 0;
};

struct OutputElement
{
  OutputElement() = default;
  constexpr explicit OutputElement(StringFragment f) : m_String(f) {}
  explicit OutputElement(OutputKind kind, int regMask) : m_RegisterMask(regMask), m_Kind(kind) {}
  explicit OutputElement(OutputKind kind, StringFragment f) : m_String(f), m_Kind(kind) {}

  StringFragment m_String;
  int            m_RegisterMask = 0;
  OutputKind     m_Kind = OutputKind::kStringLiteral;
};

class Deluxe68
{
  static constexpr size_t kLineMax = 4096;

  const char* m_InputData;
  size_t      m_InputLen;

  std::vector<OutputElement> m_OutputSchedule;

  const char* m_Filename;
  int m_LineNumber = 0;
  int m_ErrorCount = 0;
  const char* m_ParsePoint;

  StringFragment m_CurrentProcName;
  ProcedureDef m_CurrentProc;

  struct RegState
  {
    StringFragment m_AllocatingVarName;
  };

  uint32_t m_AllocatedRegs;
  uint32_t m_ReservedRegs;

  RegState m_Registers[kRegisterCount];

  struct RegAlloc
  {
    uint8_t m_RegIndex = 0;
    uint8_t m_Spilled = 0;
    int     m_AllocatedLine = 0;
  };

  std::unordered_map<StringFragment, RegAlloc> m_LiveRegs;
  std::unordered_map<StringFragment, ProcedureDef> m_Procedures;

public:
  explicit Deluxe68(const char* ifn, const char* data, size_t len);

  ~Deluxe68();

  void error(const char *fmt, ...);

  void run();
  void generateOutput(FILE* f) const;

  int errorCount() const { return m_ErrorCount; }

private:
  void parseLine(StringFragment line);

  void bufferLine(const char* line);
  void bufferLine(const std::string& line);

  void allocRegs(Tokenizer& tokenizer, TokenType regType);
  void killRegs(Tokenizer& tokenizer);
  void proc(Tokenizer& tokenizer);
  void endProc(Tokenizer& tokenizer);
  void reserve(Tokenizer& tokenizer);
  void unreserve(Tokenizer& tokenizer);
  void spill(Tokenizer& tokenizer);
  void restore(Tokenizer& tokenizer);

  void output(OutputElement elem);
  void handleRegularLine(StringFragment line);
  void newline();

  uint32_t usedRegsForProcecure(const StringFragment& procName) const;
  static void printSpill(FILE* f, uint32_t regMask);
  static void printRestore(FILE* f, uint32_t regMask);
  static void printMovemList(FILE* f, uint32_t regMask);
  void killAll();
  bool doAllocate(StringFragment id, int regIndex);

  bool dataLeft() const;
  StringFragment nextLine();

  bool expect(Tokenizer& t, TokenType type, Token* out = nullptr);
  bool accept(Tokenizer& t, TokenType type, Token* out = nullptr);

  int findFirstFree(RegisterClass regClass) const;
};

Deluxe68::Deluxe68(const char* ifn, const char* data, size_t len)
  : m_InputData(data)
  , m_InputLen(len)
  , m_Filename(ifn)
  , m_ParsePoint(data)
{
  killAll();
}

Deluxe68::~Deluxe68()
{
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
  while (dataLeft())
  {
    StringFragment line = nextLine();

    //printf("processing line: '%.*s'\n", line.length(), line.ptr());
    ++m_LineNumber;
    parseLine(line);
  }
}

void Deluxe68::parseLine(StringFragment line)
{
  StringFragment payload = skipWhitespace(line);

  if (!payload || payload[0] != '@')
  {
    handleRegularLine(line);
    return;
  }

  output(OutputElement(StringFragment("\t\t; ", 4)));
  output(OutputElement(line));
  newline();

  Tokenizer tokenizer(payload.skip(1));
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

    case TokenType::kSpill:
      spill(tokenizer);
      break;

    case TokenType::kRestore:
      restore(tokenizer);
      break;

    default:
      error("unsupported syntax: %s: %.*s\n", tokenTypeName(t.m_Type), line.length(), line.ptr());
      return;
  }
}

void Deluxe68::allocRegs(Tokenizer& tokenizer, TokenType regType)
{
  do
  {
    Token identToken;
    if (!expect(tokenizer, TokenType::kIdentifier, &identToken))
      return;

    StringFragment id = identToken.m_String;

    if (m_LiveRegs.find(id) != m_LiveRegs.end())
    {
      error("register name already in use: '%.*s'\n", id.length(), id.ptr());
      continue;
    }

    int index = -1;
    const RegisterClass regClass = regType == TokenType::kAreg ? kAddress : kData;

    if (accept(tokenizer, TokenType::kLeftParen))
    {
      Token reg;
      if (!expect(tokenizer, TokenType::kRegister, &reg))
        return;
      if (!expect(tokenizer, TokenType::kRightParen))
        return;

      index = reg.m_Register;
    }
    else
    {
      index = findFirstFree(regClass);
    }

    if (-1 == index)
    {
      error("out of %s registers (allocating %.*s)\n", registerClassName(regClass), id.length(), id.ptr());
      continue;
    }

    if (doAllocate(id, index))
    {
    }

 } while (accept(tokenizer, TokenType::kComma));

  expect(tokenizer, TokenType::kEndOfLine);
}

bool Deluxe68::doAllocate(StringFragment id, int regIndex)
{
  if ((m_AllocatedRegs | m_ReservedRegs) & (1 << regIndex))
  {
    StringFragment owner = m_Registers[regIndex].m_AllocatingVarName;
    error("register %.*s not free here (used by %.*s)\n", 2, regName(regIndex), owner.length(), owner.ptr());
    return false;
  }
  else
  {
    RegAlloc a;
    a.m_RegIndex = static_cast<uint8_t>(regIndex);
    a.m_AllocatedLine = m_LineNumber;
    m_LiveRegs.insert(std::make_pair(id, a));

    m_AllocatedRegs |= 1 << regIndex;
    m_CurrentProc.m_UsedRegs |= 1 << regIndex;

    output(OutputElement(StringFragment("\t\t; live reg ")));
    output(OutputElement(regName(regIndex)));
    output(OutputElement(StringFragment(" => ")));
    output(OutputElement(id));
    newline();

    m_Registers[regIndex].m_AllocatingVarName = id;

    return true;
  }
}

void Deluxe68::killRegs(Tokenizer& tokenizer)
{
  do
  {
    Token identToken;
    if (!expect(tokenizer, TokenType::kIdentifier, &identToken))
      return;

    StringFragment id = identToken.m_String;

    auto it = m_LiveRegs.find(id);

    if (it == m_LiveRegs.end())
    {
      error("register name not in use: '%.*s'\n", id.length(), id.ptr());
      continue;
    }

    const RegAlloc& a = it->second;
    m_AllocatedRegs &= ~(1 << a.m_RegIndex);
    m_LiveRegs.erase(it);

  } while (accept(tokenizer, TokenType::kComma));

  expect(tokenizer, TokenType::kEndOfLine);
}

void Deluxe68::proc(Tokenizer& tokenizer)
{
  if (m_CurrentProcName)
  {
    error("already inside a procedure definition ('%.*s')\n", m_CurrentProcName.length(), m_CurrentProcName.ptr());
    Tokenizer subt("");
    endProc(subt);
  }

  Token ident;
  if (expect(tokenizer, TokenType::kIdentifier, &ident))
  {
    if (m_Procedures.find(ident.m_String) != m_Procedures.end())
    {
      error("procedure '%.*s' already defined\n", ident.m_String.length(), ident.m_String.ptr());
      return;
    }

    m_CurrentProcName = ident.m_String;
    m_CurrentProc = ProcedureDef();
  }

  expect(tokenizer, TokenType::kLeftParen);

  do
  {
    Token reg;
    if (!expect(tokenizer, TokenType::kRegister, &reg))
      return;

    if (!expect(tokenizer, TokenType::kColon))
      return;

    Token identToken;
    if (!expect(tokenizer, TokenType::kIdentifier, &identToken))
      return;

    doAllocate(identToken.m_String, reg.m_Register);

  } while (accept(tokenizer, TokenType::kComma));

  expect(tokenizer, TokenType::kRightParen);
  expect(tokenizer, TokenType::kEndOfLine);

  output(OutputElement(OutputKind::kProcHeader, ident.m_String));
}

void Deluxe68::endProc(Tokenizer& tokenizer)
{
  killAll();

  if (m_CurrentProcName)
  {
    output(OutputElement(OutputKind::kProcFooter, m_CurrentProcName));
    m_Procedures.insert(std::make_pair(m_CurrentProcName, m_CurrentProc));
  }
  m_CurrentProcName = StringFragment();
  m_CurrentProc = ProcedureDef();
}

void Deluxe68::reserve(Tokenizer& tokenizer)
{
  do
  {
    Token reg;
    if (!expect(tokenizer, TokenType::kRegister, &reg))
      return;

    int regIndex = reg.m_Register;

    if ((m_AllocatedRegs | m_ReservedRegs) & (1 << regIndex))
    {
      error("register %s not free here\n", regName(regIndex));
      continue;
    }

    m_ReservedRegs |= 1 << regIndex;

  } while (accept(tokenizer, TokenType::kComma));
}

void Deluxe68::unreserve(Tokenizer& tokenizer)
{
  do
  {
    Token reg;
    if (!expect(tokenizer, TokenType::kRegister, &reg))
      return;

    int regIndex = reg.m_Register;

    if (0 == (m_ReservedRegs & (1 << regIndex)))
    {
      error("register %s not reserved here\n", regName(regIndex));
      continue;
    }

    m_ReservedRegs &= ~(1 << regIndex);

  } while (accept(tokenizer, TokenType::kComma));
}

void Deluxe68::spill(Tokenizer& tokenizer)
{
  int savedRegs = 0;

  do
  {
    Token idToken;
    StringFragment id;
    if (accept(tokenizer, TokenType::kIdentifier, &idToken))
    {
      id = idToken.m_String;
    }
    else if (expect(tokenizer, TokenType::kRegister, &idToken))
    {
      int regIndex = idToken.m_Register;
      id = m_Registers[regIndex].m_AllocatingVarName;
    }
    else
    {
      return;
    }

    auto it = m_LiveRegs.find(id);
    if (it == m_LiveRegs.end())
    {
      error("unknown register %.*s\n", id.length(), id.ptr());
      continue;
    }

    RegAlloc& alloc = it->second;

    if (alloc.m_Spilled)
    {
      error("register %.*s is already spilled\n", id.length(), id.ptr());
      continue;
    }

    alloc.m_Spilled = 1;

    int regIndex = alloc.m_RegIndex;

    m_AllocatedRegs &= ~(1 << regIndex);
    savedRegs |= 1 << regIndex;
    m_Registers[regIndex].m_AllocatingVarName = StringFragment();

  } while (accept(tokenizer, TokenType::kComma));

  output(OutputElement(OutputKind::kSpill, savedRegs));
}

void Deluxe68::restore(Tokenizer& tokenizer)
{
  int restoredRegs = 0;

  do
  {
    Token idToken;
    if (!expect(tokenizer, TokenType::kIdentifier, &idToken))
      return;
    StringFragment id = idToken.m_String;

    auto it = m_LiveRegs.find(id);
    if (it == m_LiveRegs.end())
    {
      error("unknown register %.*s\n", id.length(), id.ptr());
      continue;
    }

    RegAlloc& alloc = it->second;

    if (!alloc.m_Spilled)
    {
      error("register %.*s is not spilled\n", id.length(), id.ptr());
      continue;
    }

    int regIndex = alloc.m_RegIndex;
    if (m_AllocatedRegs &= (1 << regIndex))
    {
      StringFragment owner = m_Registers[regIndex].m_AllocatingVarName;
      error("register %.*s home slot %s is occupied by %.*s\n", id.length(), id.ptr(), regName(regIndex), owner.length(), owner.ptr());
      continue;
    }

    alloc.m_Spilled = 0;

    m_AllocatedRegs &= ~(1 << regIndex);
    restoredRegs |= 1 << regIndex;
    m_Registers[regIndex].m_AllocatingVarName = id;

  } while (accept(tokenizer, TokenType::kComma));

  output(OutputElement(OutputKind::kRestore, restoredRegs));
}

void Deluxe68::killAll()
{
  m_LiveRegs.clear();
  m_AllocatedRegs = 0;
  m_ReservedRegs = 0;
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

bool Deluxe68::accept(Tokenizer& tokenizer, TokenType type, Token* out)
{
  Token t = tokenizer.peek();
  if (t.m_Type == type)
  {
    if (out)
      *out = tokenizer.next();
    else
      tokenizer.next();
    return true;
  }
  else
  {
    return false;
  }
}

StringFragment Deluxe68::nextLine()
{
  const char* start = m_ParsePoint;
  uint32_t remain = m_InputData + m_InputLen - start;

  if (remain == 0)
    return StringFragment();

  for (uint32_t i = 0; i < remain; ++i)
  {
    if ('\n' == start[i])
    {
      StringFragment f(start, i);
      m_ParsePoint = start + i + 1;
      return f;
    }
  }

  StringFragment f(start, remain);
  m_ParsePoint = m_InputData + m_InputLen;
  return f;
}

bool Deluxe68::dataLeft() const
{
  const char* start = m_ParsePoint;
  return m_InputData + m_InputLen > start;
}

int Deluxe68::findFirstFree(RegisterClass regClass) const
{
  uint32_t classBits = 0;
  int offset = 0;
 
  switch (regClass)
  {
    case kAddress:
      classBits = (m_AllocatedRegs | m_ReservedRegs) >> 8;
      offset = kAddressBase;
      break;
    case kData:
      classBits = (m_AllocatedRegs | m_ReservedRegs) & 0xff;
      offset = kDataBase;
      break;
  }

  // TODO: PERF: Bit scan forward.
  for (int i = 0, mask = 1; i < 8; ++i, mask <<= 1)
  {
    if (0 == (classBits & mask))
      return i + offset;
  }

  return -1;
}

void Deluxe68::generateOutput(FILE* f) const
{
  for (const OutputElement& elem : m_OutputSchedule)
  {
    switch (elem.m_Kind)
    {
      case OutputKind::kStringLiteral:
      case OutputKind::kNamedRegister:
        fprintf(f, "%.*s", elem.m_String.length(), elem.m_String.ptr());
        break;
      case OutputKind::kSpill:
        printSpill(f, elem.m_RegisterMask);
        break;
      case OutputKind::kRestore:
        printRestore(f, elem.m_RegisterMask);
        break;
      case OutputKind::kProcHeader:
        fprintf(f, "\n%.*s:\n", elem.m_String.length(), elem.m_String.ptr());
        printSpill(f, usedRegsForProcecure(elem.m_String));
        break;

      case OutputKind::kProcFooter:
        printRestore(f, usedRegsForProcecure(elem.m_String));
        fprintf(f, "\t\trts\n\n");
        break;
    }
  }
}

uint32_t Deluxe68::usedRegsForProcecure(const StringFragment& procName) const
{
  auto iter = m_Procedures.find(procName);
  if (iter == m_Procedures.end())
  {
    return 0;
  }

  const ProcedureDef& procDef = iter->second;
  return procDef.m_UsedRegs;
}

void Deluxe68::printSpill(FILE* f, uint32_t regMask)
{
  fprintf(f, "\t\tmovem.l ");
  printMovemList(f, regMask);
  fprintf(f, ",-(sp)\n");
}

void Deluxe68::printRestore(FILE* f, uint32_t regMask)
{
  fprintf(f, "\t\tmovem.l (sp)+,");
  printMovemList(f, regMask);
  fprintf(f, "\n");
}

void Deluxe68::printMovemList(FILE* f, uint32_t selectedRegs)
{
  bool printed = false;
  for (int i = 0, mask = 1; i < kRegisterCount; ++i, mask <<= 1)
  {
    if (selectedRegs & mask)
    {
      fprintf(f, "%s%s", printed ? "/" : "", regName(i));
      printed = true;
    }
  }
}

void Deluxe68::output(OutputElement elem)
{
  m_OutputSchedule.push_back(elem);
}

void Deluxe68::newline()
{
  static constexpr OutputElement nl(StringFragment("\n", 1));

  m_OutputSchedule.push_back(nl);
}

void Deluxe68::handleRegularLine(StringFragment line)
{
  for (int i = 0; i < line.length(); )
  {
    if (line[i] == '@')
    {
      if (i > 0)
      {
        output(OutputElement(line.slice(i)));
      }

      line.slice(1); // Eat '@'

      int max = line.length();
      for (i = 0; i < max; ++i)
      {
        char ch = line[i];
        if (!isalnum(ch) && ch != '_')
        {
          break;
        }
      }

      StringFragment varName = line.slice(i);
      if (varName.length() > 0)
      {
        auto it = m_LiveRegs.find(varName);
        if (it == m_LiveRegs.end())
        {
          error("unknown register '%.*s' referenced\n", varName.length(), varName.ptr());
        }
        else
        {
          output(OutputElement(StringFragment(regName(it->second.m_RegIndex), 2)));
        }
      }
      else
      {
        // It's a lone '@', retain it, because they're used in macros.
        output(OutputElement(StringFragment("@", 1)));
      }

      i = 0; // restart
    }
    else
    {
      ++i;
    }
  }

  if (line.length() > 0)
  {
    output(OutputElement(line));
  }

  newline();
}

int main(int argc, char* argv[])
{
  if (argc < 3)
  {
    fprintf(stderr, "usage: deluxe68 <input> <output>\n");
    exit(1);
  }

  std::vector<char> inputData;

  {
    FILE* f = fopen(argv[1], "rb");
    if (!f)
    {
      fprintf(stderr, "can't open %s for reading\n", argv[1]);
      exit(1);
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    rewind(f);

    inputData.resize(fsize);
    fread(inputData.data(), fsize, 1, f);
    fclose(f);
  }

  Deluxe68 d(argv[1], inputData.data(), inputData.size());

  d.run();

  if (d.errorCount())
  {
    fprintf(stderr, "exiting without writing output - %d errors\n", d.errorCount());
    return 1;
  }

  if (FILE* f = fopen(argv[2], "w"))
  {
    d.generateOutput(f);
    fclose(f);
  }
  else
  {
    fprintf(stderr, "can't open %s for reading\n", argv[2]);
    exit(1);
  }

  return 0;
}
