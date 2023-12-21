
#include "ast/location.h"

#include "ast/ast.h"
#include "common/ll-common.h"
#include "context/context.h"

#include <string.h>

void Context_add_location(struct Context* context, struct Location* loc) {
  LL_APPEND(context->locations, loc);
}
struct Location* Context_build_location(
    struct Context* context,
    struct AstNode* ast,
    int line_start,
    int line_end) {
  struct Location* l = malloc(sizeof(*l));
  memset(l, 0, sizeof(*l));
  l->ast = ast;
  l->line_start = line_start;
  l->line_end = line_end;
  Context_add_location(context, l);
  return l;
}
struct Location*
Context_get_location(struct Context* context, struct AstNode* ast) {
  if(!ast) return NULL;
  LL_FOREACH(context->locations, loc) {
    if(loc->ast == ast) return loc;
  }
  return NULL;
}
