#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
void* c_allocate(int64_t n) {
  return malloc(n);
}
void c_deallocate(void* p) {
  free(p);
}
