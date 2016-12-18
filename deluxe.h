#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

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
  kProcFooter,
  kStackVar,
  kLineDirective
};

struct ProcedureDef
{
  uint32_t m_UsedRegs = 0;
  uint32_t m_InputRegs = 0;
};

struct OutputElement
{
  OutputElement() = default;
  constexpr explicit OutputElement(StringFragment f) : m_String(f) {}
  explicit OutputElement(OutputKind kind, int intVal) : m_IntValue(intVal), m_Kind(kind) {}
  explicit OutputElement(OutputKind kind, StringFragment f) : m_String(f), m_Kind(kind) {}

  StringFragment m_String;
  int            m_IntValue = 0;
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

  bool m_EmitLineDirectives = false;
  int m_CurrentOutputLine = 0;

  StringFragment m_CurrentProcName;
  ProcedureDef m_CurrentProc;

  struct RegState
  {
    static constexpr uint32_t kFlagAllocated = 1 << 0;
    static constexpr uint32_t kFlagReserved  = 1 << 1;

    uint32_t                    m_Flags = 0;
    StringFragment              m_AllocatingVarName;
    std::vector<StringFragment> m_SpilledVars;

    bool isAllocated() const { return 0 != (m_Flags & kFlagAllocated); }
    bool isReserved() const { return 0 != (m_Flags & kFlagReserved); }
    bool isInUse() const { return 0 != (m_Flags & (kFlagReserved | kFlagAllocated)); }

    void setAllocated(bool state)
    {
      if (state)
        m_Flags |= kFlagAllocated;
      else
        m_Flags &= ~kFlagAllocated;
    }

    void setReserved(bool state)
    {
      if (state)
        m_Flags |= kFlagReserved;
      else
        m_Flags &= ~kFlagReserved;
    }

    void reset()
    {
      m_Flags = 0;
      m_AllocatingVarName = StringFragment();
      m_SpilledVars.clear();
    }

    void spill()
    {
      setAllocated(false);
      m_SpilledVars.push_back(m_AllocatingVarName);
      m_AllocatingVarName = StringFragment();
    }

    void restore()
    {
    }

    void handleRename(StringFragment idOld, StringFragment idNew)
    {
      if (m_AllocatingVarName == idOld)
      {
        m_AllocatingVarName = idNew;
      }

      for (auto& str : m_SpilledVars)
      {
        if (str == idOld)
        {
          str = idNew;
        }
      }
    }
  };

  RegState m_Registers[kRegisterCount];

  struct RegAlloc
  {
    uint8_t m_RegIndex = 0;
    uint8_t m_Spilled = 0;
    int     m_StackSlot = 0;
    int     m_AllocatedLine = 0;
  };

  int m_SpillStackDepth = 0;
  std::unordered_map<StringFragment, RegAlloc> m_LiveRegs;
  std::unordered_map<StringFragment, ProcedureDef> m_Procedures;

public:
  explicit Deluxe68(const char* ifn, const char* data, size_t len, bool emitLineDirectives);

  ~Deluxe68();

  void error(const char *fmt, ...);
  void errorForLine(int line, const char *fmt, ...);

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
  void rename(Tokenizer& tokenizer);

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

