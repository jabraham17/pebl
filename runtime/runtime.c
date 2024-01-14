#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <wchar.h>

void* c_allocate(int64_t n);

__attribute__((noreturn))
void pebl_panic(wchar_t* message) {
  fwprintf(stderr, L"%ls\n", message);
  abort();
}

wchar_t* intToString(int64_t i) {
  wchar_t* buf;
  int buf_size = sizeof(*buf)*16;
  buf = c_allocate(buf_size);
  swprintf(buf, buf_size, L"%ld", i);
  return buf;
}
int64_t stringToInt(wchar_t* s) {
  wchar_t* end;
  return (int64_t)wcstoll(s, &end, 10);
}
wchar_t* ptrToString(void* p) {
  wchar_t* buf;
  int buf_size = sizeof(*buf)*16;
  buf = c_allocate(buf_size);
  swprintf(buf, buf_size, L"%p", p);
  return buf;
}

