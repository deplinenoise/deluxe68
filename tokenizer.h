#pragma once

enum class TokenType
{
  kAreg,
  kDreg,
  kKill,
  kReserve,
  kUnreserve,
  kRegisterName,
  kIdentifier,
  kLeftParen,
  kRightParen,
  kProc,
  kEndProc,
  kComma,
  kColon,
  kEndOfLine,
  kUnknown,
  kInvalid
};

const char* tokenTypeName(TokenType tt);

struct Token
{
  TokenType m_Type = TokenType::kInvalid;
  const char* m_Start = nullptr;
  const char* m_End = nullptr;
};

class Tokenizer
{
  const char* m_Ptr;
  Token m_Curr;

public:
  explicit Tokenizer(const char* p);

  Token peek();
  Token next();

private:
  void decodeNext(Token* t);
};

const char* skipWhitespace(const char* p);

