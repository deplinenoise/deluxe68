#pragma once

#include "stringfragment.h"

enum class TokenType
{
  kAreg,
  kDreg,
  kKill,
  kReserve,
  kUnreserve,
  kRegister,
  kIdentifier,
  kLeftParen,
  kRightParen,
  kProc,
  kCProc,
  kEndProc,
  kComma,
  kColon,
  kEndOfLine,
  kSpill,
  kRestore,
  kRename,
  kUnknown,
  kInvalid,
  kCount
};

const char* tokenTypeName(TokenType tt);

struct Token
{
  Token() = default;

  Token(TokenType type, StringFragment str)
    : m_Type(type)
    , m_String(str)
  {}

  Token(TokenType type, int registerIndex)
    : m_Type(type)
    , m_Register(registerIndex)
  {}

  TokenType m_Type = TokenType::kInvalid;
  int m_Register = 0;
  StringFragment m_String;
};

class Tokenizer
{
  StringFragment m_Remain;
  Token m_Curr;

public:
  explicit Tokenizer(StringFragment p);

  Token peek();
  Token next();

private:
  Token decodeNext();
};

StringFragment skipWhitespace(StringFragment f);

