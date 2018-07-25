#include "d68test.h"
#include "deluxe.h"

std::string DeluxeTest::xform(const char* in, bool line_directives)
{
  Deluxe68 d68("<unittest>", in, strlen(in), line_directives, false);
  d68.run();
  d68.generateOutput([](const char* buf, size_t len, void* user_data)
  {
    std::string* s = static_cast<std::string*>(user_data);
    s->insert(s->end(), buf, buf + len);
  }, &output);

  return filter(output);
}

std::string DeluxeTest::filter(const std::string& in)
{
  std::string::size_type pos = 0;
  std::string output;
  while (pos < in.size())
  {
    std::string::size_type eolpos = in.find('\n', pos);
    if (eolpos == std::string::npos)
    {
      eolpos = in.size() - 1;
    }

    std::string line(in.substr(pos, eolpos - pos + 1));

    if (!is_ws_or_comment(line))
    {
      output.insert(output.end(), line.begin(), line.end());
    }

    pos = eolpos + 1;
  }

  return output;
}

bool DeluxeTest::is_ws_or_comment(const std::string& line)
{
  const char* p = line.c_str();
  for (;;)
  {
    char ch = *p++;

    if (isspace(ch))
      continue;

    if (ch == ';' || ch == '\0')
      return true;

    return false;
  }
}
