#include "cg-debug.h"

#include "codegen/codegen-llvm.h"

#include <stdlib.h>
#include <string.h>

struct di_function* allocate_di_function() {
  struct di_function* di = malloc(sizeof(*di));
  memset(di, 0, sizeof(*di));
  return di;
}
