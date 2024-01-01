#include "context/context.h"

#include "ast/Type.h"
#include "ast/ast.h"
#include "ast/location.h"
#include "ast/scope-resolve.h"
#include "common/bsstring.h"
#include "parser/parser.h"

#include <locale.h>
#include <string.h>

struct Context* Context_allocate() {
  struct Context* context = malloc(sizeof(*context));
  return context;
}
void Context_init(struct Context* context, char* filename) {
  memset(context, 0, sizeof(*context));
  context->filename = bsstrdup(filename);
  setlocale(LC_CTYPE, "");
}

void BREAKPOINT() {}
