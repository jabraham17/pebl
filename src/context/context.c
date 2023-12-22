#include "context/context.h"

#include "ast/TypeTable.h"
#include "ast/ast.h"
#include "ast/location.h"
#include "ast/scope-resolve.h"
#include "common/bsstring.h"
#include "parser/parser.h"

#include <string.h>
#include "common/bsstring.h"

struct Context* Context_allocate() {
  struct Context* context = malloc(sizeof(*context));
  return context;
}
void Context_init(struct Context* context, char* filename) {
  memset(context, 0, sizeof(*context));
  context->filename = bsstrdup(filename);
}
