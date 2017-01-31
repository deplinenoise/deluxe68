#pragma once
#include "gtest/gtest.h"

class DeluxeTest : public ::testing::Test
{
protected:
  std::string output;

public:
  DeluxeTest()
  {}

protected:
  std::string xform(const char* in, bool line_directives = false);

  std::string filter(const std::string& in);

  bool is_ws_or_comment(const std::string& line);
};

