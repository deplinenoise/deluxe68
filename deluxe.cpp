#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>

#include <set>
#include <vector>

enum Register : uint8_t
{
  D0, D1, D2, D3, D4, D5, D6, D7,
  A0, A1, A2, A3, A4, A5, A6, A7
};

const char* name(Register r)
{
  static const char* registerNames[] = {
    "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
    "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7"
  };

  return registerNames[(int)r];
}

class Deluxe68
{
  static constexpr size_t kLineMax = 4096;

  FILE*       m_Input;
  const char* m_Filename;
  int         m_LineNumber = 0;
  int         m_ErrorCount = 0;

  std::set<Register> m_FreeDataRegs;
  std::set<Register> m_FreeAddrRegs;

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

  static const char* skipWhitespace(const char* p);

  void bufferLine(const char* line);
};

Deluxe68::Deluxe68(const char* fn)
  : m_Input(fopen(fn, "r"))
  , m_Filename(fn)
{
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

  ++p;

}

const char* Deluxe68::skipWhitespace(const char* p)
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
