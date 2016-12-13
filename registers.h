#pragma once

enum Registers
{
  kD0 =  0, kD1 =  1, kD2 =  2, kD3 =  3,
  kD4 =  4, kD5 =  5, kD6 =  6, kD7 =  7,
  kA0 =  8, kA1 =  9, kA2 = 10, kA3 = 11,
  kA4 = 12, kA5 = 13, kA6 = 14, kA7 = 15,

  kRegisterCount = 16,

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
