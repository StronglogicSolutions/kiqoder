#include "kiqoder.hpp"


int main(int argc, char* argv[])
{
  Kiqoder::FileHandler{[](int32_t i, uint8_t* p, size_t s) { (void)(0); }}.processPacket(new uint8_t[12]{}, 12);
  return 0;
}

