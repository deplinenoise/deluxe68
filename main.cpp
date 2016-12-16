#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <vector>

#include "deluxe.h"

static void usage()
{
  fprintf(stderr, "usage: deluxe68 [options] <input> <output>\n");
  fprintf(stderr, "options:\n");
  fprintf(stderr, "  -l     emit tbl_line directives\n");
  exit(1);
}

int main(int argc, char* argv[])
{
  bool emitLineDirectives = false;
  int positionalCount = 0;
  const char* positionals[2] = { nullptr, nullptr };

  for (int i = 1; i < argc; ++i)
  {
    if ('-' == argv[i][0])
    {
      if (0 == strcmp("-l", argv[i]))
      {
        emitLineDirectives = true;
      }
      else
      {
        fprintf(stderr, "invalid option: %s\n", argv[i]);
        usage();
      }
    }
    else if (positionalCount < 2)
    {
      positionals[positionalCount++] = argv[i];
    }
    else
    {
      usage();
    }
  }

  if (positionalCount < 2)
    usage();

  std::vector<char> inputData;

  {
    FILE* f = fopen(positionals[0], "rb");
    if (!f)
    {
      fprintf(stderr, "can't open %s for reading\n", positionals[0]);
      exit(1);
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    rewind(f);

    inputData.resize(fsize);
    fread(inputData.data(), fsize, 1, f);
    fclose(f);
  }

  Deluxe68 d(positionals[0], inputData.data(), inputData.size(), emitLineDirectives);

  d.run();

  if (d.errorCount())
  {
    fprintf(stderr, "exiting without writing output - %d errors\n", d.errorCount());
    return 1;
  }

  if (FILE* f = fopen(positionals[1], "w"))
  {
    d.generateOutput(f);
    fclose(f);
  }
  else
  {
    fprintf(stderr, "can't open %s for reading\n", positionals[1]);
    exit(1);
  }

  return 0;
}
