#include "deluxe.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

Deluxe68::Deluxe68(const char* ifn, const char* data, size_t len, bool emitLineDirectives)
  : m_InputData(data)
  , m_InputLen(len)
  , m_Filename(ifn)
  , m_ParsePoint(data)
  , m_EmitLineDirectives(emitLineDirectives)
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

  int inputRegMask = 0;

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
  expect(tokenizer, TokenType::kEndOfLine);

  output(OutputElement(OutputKind::kProcHeader, ident.m_String));

  m_CurrentProc.m_InputRegs = inputRegMask;
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

    if (m_Registers[regIndex].isReserved())
    {
      error("register %s not reserved here\n", regName(regIndex));
      continue;
    }

    m_Registers[regIndex].setReserved(false);

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

    alloc.m_StackSlot = m_SpillStackDepth++;
    alloc.m_Spilled = 1;

    int regIndex = alloc.m_RegIndex;

    savedRegs |= 1 << regIndex;

    m_Registers[regIndex].spill();

  } while (accept(tokenizer, TokenType::kComma));

  if (0 != savedRegs)
  {
    output(OutputElement(OutputKind::kSpill, savedRegs));
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
  for (const OutputElement& elem : m_OutputSchedule)
  {
    switch (elem.m_Kind)
    {
      case OutputKind::kStringLiteral:
      case OutputKind::kNamedRegister:
        fprintf(f, "%.*s", elem.m_String.length(), elem.m_String.ptr());
        break;
      case OutputKind::kSpill:
        printSpill(f, elem.m_IntValue);
        break;
      case OutputKind::kRestore:
        printRestore(f, elem.m_IntValue);
        break;
      case OutputKind::kProcHeader:
        fprintf(f, "\n%.*s:\n", elem.m_String.length(), elem.m_String.ptr());
        printSpill(f, usedRegsForProcecure(elem.m_String));
        break;
      case OutputKind::kProcFooter:
        printRestore(f, usedRegsForProcecure(elem.m_String));
        fprintf(f, "\t\trts\n\n");
        break;
      case OutputKind::kStackVar:
        // FIXME: This is broken for word references, needs an additional +2, but we don't know that.
        // Similarily bytes need a +3.
        fprintf(f, "%d(sp)", elem.m_IntValue);
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
  return procDef.m_UsedRegs & ~(procDef.m_InputRegs);
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
