#include "deluxe.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

Deluxe68::Deluxe68(const char* ifn, const char* data, size_t len, bool emitLineDirectives, bool procSections)
  : m_InputData(data)
  , m_InputLen(len)
  , m_Filename(ifn)
  , m_ParsePoint(data)
  , m_EmitLineDirectives(emitLineDirectives)
  , m_ProcSections(procSections)
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

void Deluxe68::errorForLine(int lineNumber, const char *fmt, ...)
{
  if (lineNumber)
    fprintf(stderr, "%s(%d): ", m_Filename, lineNumber);
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
  int lineDelta = -1;
  while (dataLeft())
  {
    StringFragment line = nextLine();

    if (m_EmitLineDirectives)
    {
      int currentLineDelta = m_CurrentOutputLine - m_LineNumber;

      if (currentLineDelta != lineDelta)
      {
        output(OutputElement(OutputKind::kLineDirective, m_LineNumber + 1));
        lineDelta = currentLineDelta;
      }
    }

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
      proc(tokenizer, false);
      break;

    case TokenType::kCProc:
      proc(tokenizer, true);
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

    case TokenType::kRename:
      rename(tokenizer);
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
      for (int i = 0; i < kRegisterCount; ++i)
      {
        if (m_Registers[i].isAllocated())
        {
          StringFragment owner = m_Registers[i].m_AllocatingVarName;
          int lineNo = 0;
          const auto it = m_LiveRegs.find(owner);
          if (it != m_LiveRegs.end())
          {
            lineNo = it->second.m_AllocatedLine;
          }
          errorForLine(lineNo, "%s allocated: %.*s\n", regName(i), owner.length(), owner.ptr());
        }
        else if (m_Registers[i].isReserved())
        {
          error("%s: (reserved)\n", regName(i));
        }
      }
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
  if (m_Registers[regIndex].isInUse())
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

    m_Registers[regIndex].setAllocated(true);
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
    m_Registers[a.m_RegIndex].setAllocated(false);
    m_LiveRegs.erase(it);

  } while (accept(tokenizer, TokenType::kComma));

  expect(tokenizer, TokenType::kEndOfLine);
}

void Deluxe68::proc(Tokenizer& tokenizer, bool saveInputs)
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

  int inputRegMask = 0;

  // Allow just proc <ident>, OR proc <ident> (<signature>)
  if (accept(tokenizer, TokenType::kLeftParen))
  {
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

      inputRegMask |= 1 << reg.m_Register;

      doAllocate(identToken.m_String, reg.m_Register);

    } while (accept(tokenizer, TokenType::kComma));

    expect(tokenizer, TokenType::kRightParen);
  }

  expect(tokenizer, TokenType::kEndOfLine);

  output(OutputElement(OutputKind::kProcHeader, ident.m_String));

  m_CurrentProc.m_InputRegs = inputRegMask;
  m_CurrentProc.m_SaveInputRegs = saveInputs;
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

    if (m_Registers[regIndex].isInUse())
    {
      error("register %s not free here\n", regName(regIndex));
      continue;
    }

    m_Registers[regIndex].setReserved(true);
    m_CurrentProc.m_UsedRegs |= 1 << regIndex;

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

    if (!m_Registers[regIndex].isReserved())
    {
      error("register %s not reserved here\n", regName(regIndex));
      continue;
    }

    m_Registers[regIndex].setReserved(false);

  } while (accept(tokenizer, TokenType::kComma));
}

// This unfortunate bit of complexity is needed to compute the correct stack
// slots for spilled registers, as movem.l will always write registers in a
// particular order to memory.
// 
// Therefore, we can't know the spilled slot on the stack until we've looked at
// all the spills involved.
struct Deluxe68::PendingRegisterSpill
{
  int m_RegisterIndex;
  RegAlloc* m_RegAlloc;
};

// Note this sorts in the opposite order, so we assign slots to the first things spilled, e.g. a6,a5,a4...d1,d0
static bool operator<(const Deluxe68::PendingRegisterSpill& lhs, const Deluxe68::PendingRegisterSpill& rhs)
{
  return lhs.m_RegisterIndex > rhs.m_RegisterIndex;
}

void Deluxe68::spill(Tokenizer& tokenizer)
{
  int savedRegMask = 0;
  int savedRegCount = 0;
  PendingRegisterSpill pendingSpills[kRegisterCount];

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

      // Track this register for saving in the procedure.
      m_CurrentProc.m_UsedRegs |= 1 << regIndex;

      if (!m_Registers[regIndex].isInUse())
      {
        continue;
      }

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
    alloc.m_StackSlot = -1;

    int regIndex = alloc.m_RegIndex;

    savedRegMask |= 1 << regIndex;
    pendingSpills[savedRegCount].m_RegisterIndex = regIndex;
    pendingSpills[savedRegCount].m_RegAlloc = &alloc;
    savedRegCount++;

  } while (accept(tokenizer, TokenType::kComma));

  if (0 != savedRegMask)
  {
    // Assign stack slots depending on registers involved in the movem.

    // It's enough to just sort on the register index here, because movem.l
    // will always write d0,d1,d2...,a0,a1,... to memory. The final address
    // pointer ends up pointing at the lowest d-register written.
    std::sort(pendingSpills, pendingSpills + savedRegCount);

    for (int i = 0; i < savedRegCount; ++i)
    {
      pendingSpills[i].m_RegAlloc->m_StackSlot = m_SpillStackDepth++;
      m_Registers[pendingSpills[i].m_RegisterIndex].spill();
    }

    output(OutputElement(OutputKind::kSpill, savedRegMask));
  }
}

void Deluxe68::restore(Tokenizer& tokenizer)
{
  int restoredRegs = 0;

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
      int registerIndex = idToken.m_Register;
      if (m_Registers[registerIndex].m_SpilledVars.empty())
      {
        // This is more of an information tidbit to the programmer.
        // We allow spill to take any regs, and just don't spill them if they're unused.
        // This is symmetrical
        //error("register %s not spilled here\n", regName(registerIndex));
        continue;
      }
      id = m_Registers[registerIndex].m_SpilledVars.back();
      m_Registers[registerIndex].m_SpilledVars.pop_back();
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

    if (!alloc.m_Spilled)
    {
      error("register %.*s is not spilled\n", id.length(), id.ptr());
      continue;
    }

    int regIndex = alloc.m_RegIndex;

    if (m_Registers[regIndex].isInUse())
    {
      if (m_Registers[regIndex].isAllocated())
      {
        StringFragment owner = m_Registers[regIndex].m_AllocatingVarName;
        error("register %.*s home slot %s is occupied by %.*s\n", id.length(), id.ptr(), regName(regIndex), owner.length(), owner.ptr());
      }
      else
      {
        error("register %.*s home slot %s is reserved\n", id.length(), id.ptr(), regName(regIndex));
      }
      continue;
    }

    alloc.m_Spilled = 0;

    m_Registers[regIndex].setAllocated(true);
    restoredRegs |= 1 << regIndex;
    m_Registers[regIndex].m_AllocatingVarName = id;

  } while (accept(tokenizer, TokenType::kComma));

  if (0 != restoredRegs)
  {
    output(OutputElement(OutputKind::kRestore, restoredRegs));
  }
}

void Deluxe68::rename(Tokenizer& tokenizer)
{
  Token tokenOld, tokenNew;

  if (!expect(tokenizer, TokenType::kIdentifier, &tokenOld))
    return;

  if (!expect(tokenizer, TokenType::kIdentifier, &tokenNew))
    return;

  StringFragment idOld = tokenOld.m_String;
  StringFragment idNew = tokenNew.m_String;

  if (!expect(tokenizer, TokenType::kEndOfLine))
    return;

  if (m_LiveRegs.find(idNew) != m_LiveRegs.end())
  {
    error("id %.*s already allocated\n", idNew.length(), idNew.ptr());
    return;
  }

  auto it = m_LiveRegs.find(idOld);
  if (it == m_LiveRegs.end())
  {
    error("id %.*s not allocated\n", idOld.length(), idOld.ptr());
    return;
  }

  RegAlloc alloc = it->second;

  m_LiveRegs.erase(it);
  m_LiveRegs.insert(std::make_pair(idNew, alloc));

  m_Registers[alloc.m_RegIndex].handleRename(idOld, idNew);
}

void Deluxe68::killAll()
{
  m_LiveRegs.clear();

  for (int i = 0; i < kRegisterCount; ++i)
  {
    m_Registers[i].reset();
  }

  m_Registers[kA7].setReserved(true);
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
  int top = 0;
  int bot = 0;
 
  switch (regClass)
  {
    case kAddress:
      top = kA7;      // User can explicitly unreserve to make it available.
      bot = kA0;
      break;
    case kData:
      top = kD7;
      bot = kD0;
      break;
  }

  // Allocate from the top down.
  for (int i = top; i >= bot; --i)
  {
    if (!m_Registers[i].isInUse())
      return i;
  }

  return -1;
}

void Deluxe68::generateOutput(FILE* f) const
{
  generateOutput([](const char* n, size_t len, void* user_data)
  {
    fwrite(n, 1, len, (FILE*)user_data);
  }, f);
}

void Deluxe68::generateOutput(PrintCallback* cb, void* user_data) const
{
  m_PrintCallback = cb;
  m_PrintData = user_data;

  for (const OutputElement& elem : m_OutputSchedule)
  {
    switch (elem.m_Kind)
    {
      case OutputKind::kStringLiteral:
      case OutputKind::kNamedRegister:
        outf("%.*s", elem.m_String.length(), elem.m_String.ptr());
        break;
      case OutputKind::kSpill:
        printSpill(elem.m_IntValue);
        break;
      case OutputKind::kRestore:
        printRestore(elem.m_IntValue);
        break;
      case OutputKind::kProcHeader:
        if (m_ProcSections)
          outf("\t\tsection\tproc_%.*s,code\n", elem.m_String.length(), elem.m_String.ptr());
        outf("%.*s:\n", elem.m_String.length(), elem.m_String.ptr());
        printSpill(usedRegsForProcecure(elem.m_String));
        break;
      case OutputKind::kProcFooter:
        printRestore(usedRegsForProcecure(elem.m_String));
        outf("\t\trts\n");
        break;
      case OutputKind::kStackVar:
        // FIXME: This is broken for word references, needs an additional +2, but we don't know that.
        // Similarily bytes need a +3.
        outf("%d(sp)", elem.m_IntValue);
        break;
      case OutputKind::kLineDirective:
        outf("\t\ttbl_line %d %s\n", elem.m_IntValue, m_Filename);
        break;
    }
  }

  m_PrintCallback = nullptr;
  m_PrintData = nullptr;
}

uint32_t Deluxe68::usedRegsForProcecure(const StringFragment& procName) const
{
  auto iter = m_Procedures.find(procName);
  if (iter == m_Procedures.end())
  {
    return 0;
  }

  const ProcedureDef& procDef = iter->second;
  if (procDef.m_SaveInputRegs)
    return procDef.m_UsedRegs | procDef.m_InputRegs;
  else
    return procDef.m_UsedRegs & ~(procDef.m_InputRegs);
}

void Deluxe68::printSpill(uint32_t regMask) const
{
  if (regMask)
  {
    outf("\t\tmovem.l ");
    printMovemList(regMask);
    outf(",-(sp)\n");
  }
}

void Deluxe68::printRestore(uint32_t regMask) const
{
  if (regMask)
  {
    outf("\t\tmovem.l (sp)+,");
    printMovemList(regMask);
    outf("\n");
  }
}

void Deluxe68::printMovemList(uint32_t selectedRegs) const
{
  bool printed = false;
  for (int i = 0, mask = 1; i < kRegisterCount; ++i, mask <<= 1)
  {
    if (selectedRegs & mask)
    {
      outf("%s%s", printed ? "/" : "", regName(i));
      printed = true;
    }
  }
}

void Deluxe68::output(OutputElement elem)
{
  m_OutputSchedule.push_back(elem);

  switch (elem.m_Kind)
  {
    case OutputKind::kStringLiteral:
      for (char ch : elem.m_String)
      {
        if ('\n' == ch)
          ++m_CurrentOutputLine;
      }
      break;

    case OutputKind::kSpill:
    case OutputKind::kRestore:
      ++m_CurrentOutputLine;
      break;

    case OutputKind::kProcHeader:
      m_CurrentOutputLine += 2;
      break;
    case OutputKind::kProcFooter:
      m_CurrentOutputLine += 2;
      break;
    default:
      break;
  }
}

void Deluxe68::newline()
{
  static constexpr OutputElement nl(StringFragment("\n", 1));

  m_OutputSchedule.push_back(nl);
  ++m_CurrentOutputLine;
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
          continue;
        }
        const RegAlloc& alloc = it->second;

        if (alloc.m_Spilled)
        {
          // Use stack position
          output(OutputElement(OutputKind::kStackVar, 4 * (m_SpillStackDepth - alloc.m_StackSlot - 1)));
        }
        else
        {
          // Live.
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
    else if (line[i] == ';')
    {
      break;
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

void Deluxe68::outf(const char* fmt, ...) const
{
  char line[1024];
  va_list a;
  va_start(a, fmt);
  int len = vsnprintf(line, sizeof line, fmt, a);
  va_end(a);
  m_PrintCallback(line, len, m_PrintData);
}
