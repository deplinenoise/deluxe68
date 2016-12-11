#include "registers.h"

const char* regName(int index)
{
  static const char* lut[]
  {
    "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
  };

  return lut[index];
}

RegisterClass registerClass(int index)
{
  return index < kAddressBase ? kData : kAddress;
}

const char* registerClassName(RegisterClass cls)
{
  return cls == kAddress ? "address" : "data";
}
