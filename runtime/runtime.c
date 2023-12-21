#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

void* c_allocate(int64_t n);

int8_t* intToString(int64_t i) {
  char* buf = c_allocate(16);
  sprintf(buf, "%ld", i);
  return  (int8_t*)buf;
}
int64_t stringToInt(int8_t* s) {
  return atoi((char*)s);
}
int8_t* ptrToString(void* p) {
  char* buf = c_allocate(16);
  sprintf(buf, "%p", p);
  return (int8_t*)buf;
}

