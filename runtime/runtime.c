#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
void print(int8_t* s) {
  printf("%s", (char*)s);
}
void println(int8_t* s) {
  print(s);
  printf("\n");
}
void* allocate(int64_t n) {
  return malloc(n);
}
int8_t* intToString(int64_t i) {
  char* buf = allocate(16);
  sprintf(buf, "%ld", i);
  return  (int8_t*)buf;
}
int64_t stringToInt(int8_t* s) {
  return atoi((char*)s);
}
int8_t* ptrToString(void* p) {
  char* buf = allocate(16);
  sprintf(buf, "%p", p);
  return (int8_t*)buf;
}

extern int64_t _bs_main_entry(int8_t** args, int64_t nargs);
int main(int argc, char** argv) {
  return _bs_main_entry((int8_t**)argv, (int64_t)argc);
}
