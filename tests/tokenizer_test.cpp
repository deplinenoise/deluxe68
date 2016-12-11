#include "tokenizer.h"
#include "gtest/gtest.h"

TEST(Tokenizer, Identifiers)
{
  Tokenizer tokenizer(StringFragment("abc"));

  Token t0 = tokenizer.next();
  EXPECT_EQ(TokenType::kIdentifier, t0.m_Type);
  Token t1 = tokenizer.next();
  EXPECT_EQ(TokenType::kEndOfLine, t1.m_Type);
}
