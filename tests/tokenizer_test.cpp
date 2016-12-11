#include "tokenizer.h"
#include "registers.h"
#include "gtest/gtest.h"

TEST(Tokenizer, Identifiers)
{
  Tokenizer tokenizer(StringFragment("abc"));

  Token t0 = tokenizer.next();
  EXPECT_EQ(TokenType::kIdentifier, t0.m_Type);
  Token t1 = tokenizer.next();
  EXPECT_EQ(TokenType::kEndOfLine, t1.m_Type);
}

TEST(Tokenizer, IdentifierssAndWhiteSpace)
{
  Tokenizer tokenizer(StringFragment("  a_c bb1 c  "));

  Token t0 = tokenizer.next();
  EXPECT_EQ(TokenType::kIdentifier, t0.m_Type);
  EXPECT_EQ(StringFragment("a_c"), t0.m_String);
  Token t1 = tokenizer.next();
  EXPECT_EQ(TokenType::kIdentifier, t1.m_Type);
  EXPECT_EQ(StringFragment("bb1"), t1.m_String);
  Token t2 = tokenizer.next();
  EXPECT_EQ(TokenType::kIdentifier, t2.m_Type);
  EXPECT_EQ(StringFragment("c"), t2.m_String);
  Token t3 = tokenizer.next();
  EXPECT_EQ(TokenType::kEndOfLine, t3.m_Type);
}

TEST(Tokenizer, RegisterNames)
{
  Tokenizer tokenizer(StringFragment("  a0 a8 d7 d4  "));

  Token t0 = tokenizer.next();
  EXPECT_EQ(TokenType::kRegister, t0.m_Type);
  EXPECT_EQ(kA0, t0.m_Register);

  Token t1 = tokenizer.next();
  EXPECT_EQ(TokenType::kIdentifier, t1.m_Type);
  EXPECT_EQ(StringFragment("a8"), t1.m_String);

  Token t2 = tokenizer.next();
  EXPECT_EQ(TokenType::kRegister, t2.m_Type);
  EXPECT_EQ(kD7, t2.m_Register);

  Token t3 = tokenizer.next();
  EXPECT_EQ(TokenType::kRegister, t3.m_Type);
  EXPECT_EQ(kD4, t3.m_Register);

  Token t4 = tokenizer.next();
  EXPECT_EQ(TokenType::kEndOfLine, t4.m_Type);
}

TEST(Tokenizer, Punctuation)
{
  Tokenizer tokenizer(StringFragment(" (:, )  "));

  Token t0 = tokenizer.next();
  EXPECT_EQ(TokenType::kLeftParen, t0.m_Type);
  Token t1 = tokenizer.next();
  EXPECT_EQ(TokenType::kColon, t1.m_Type);
  Token t2 = tokenizer.next();
  EXPECT_EQ(TokenType::kComma, t2.m_Type);
  Token t3 = tokenizer.next();
  EXPECT_EQ(TokenType::kRightParen, t3.m_Type);
  Token t4 = tokenizer.next();
  EXPECT_EQ(TokenType::kEndOfLine, t4.m_Type);
}

TEST(Tokenizer, Comments)
{
  Tokenizer tokenizer(StringFragment(" ) ; whatever  "));

  Token t0 = tokenizer.next();
  EXPECT_EQ(TokenType::kRightParen, t0.m_Type);
  Token t1 = tokenizer.next();
  EXPECT_EQ(TokenType::kEndOfLine, t1.m_Type);
}

TEST(Tokenizer, Keywords)
{
  Tokenizer tokenizer(StringFragment(" spill restore aregdreg areg dreg kill reserve proc endproc "));

  static const TokenType expected[] =
  {
    TokenType::kSpill,
    TokenType::kRestore,
    TokenType::kIdentifier, // Make sure run-together keywords don't lex as keywords
    TokenType::kAreg,
    TokenType::kDreg,
    TokenType::kKill,
    TokenType::kReserve,
    TokenType::kProc,
    TokenType::kEndProc
  };

  for (size_t i = 0; i < sizeof(expected)/sizeof(expected[0]); ++i)
  {
    Token t0 = tokenizer.next();
    EXPECT_EQ(expected[i], t0.m_Type);
  }

  Token eol = tokenizer.next();
  EXPECT_EQ(TokenType::kEndOfLine, eol.m_Type);
}
