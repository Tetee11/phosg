#include <assert.h>
#include <stdio.h>

#include "Encoding.hh"


int main(int argc, char* argv[]) {
  assert(bswap16(0x0123) == 0x2301);
  assert(bswap32(0x01234567) == 0x67452301);
  assert(bswap64(0x0123456789ABCDEF) == 0xEFCDAB8967452301);

  assert(bswap16(-1) == (uint16_t)-1);
  assert(bswap32(-1) == (uint32_t)-1);
  assert(bswap64(-1) == (uint64_t)-1);

  assert(bswap16(-2) == (uint16_t)-257);
  assert(bswap32(-2) == (uint32_t)-16777217);
  assert(bswap64(-2) == (uint64_t)-72057594037927937);

  printf("%s: all tests passed\n", argv[0]);
  return 0;
}
