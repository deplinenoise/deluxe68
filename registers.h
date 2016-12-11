#pragma once

enum Registers
{
  kD0 =  0, kD1 =  1, kD2 =  2, kD3 =  3,
  kD4 =  4, kD5 =  5, kD6 =  6, kD7 =  7,
  kA0 =  8, kA1 =  9, kA2 = 10, kA3 = 11,
  kA4 = 12, kA5 = 13, kA6 = 14, kA7 = 15,

  kRegisterCount = 16,

  // Data registers
  kD0Flag          = 1 << kD0,
  kD1Flag          = 1 << kD1,
  kD2Flag          = 1 << kD2,
  kD3Flag          = 1 << kD3,
  kD4Flag          = 1 << kD4,
  kD5Flag          = 1 << kD5,
  kD6Flag          = 1 << kD6,
  kD7Flag          = 1 << kD7,

  // Address registers
  kA0Flag          = 1 << kA0,
  kA1Flag          = 1 << kA1,
  kA2Flag          = 1 << kA2,
  kA3Flag          = 1 << kA3,
  kA4Flag          = 1 << kA4,
  kA5Flag          = 1 << kA5,
  kA6Flag          = 1 << kA6,
  kA7Flag          = 1 << kA7,

  kDataRegisterMask = 0x00ff,
  kAddrRegisterMask = 0xff00,

  kDataBase         = kD0,
  kAddressBase      = kA0,
};

enum RegisterClass
{
  kData,
  kAddress
};

RegisterClass registerClass(int index);
const char* registerClassName(RegisterClass cls);

const char* regName(int index);
