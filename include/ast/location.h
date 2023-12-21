
#ifndef LOCATION_H_
#define LOCATION_H_

struct Context;

struct AstNode;
struct Location {
  struct AstNode* ast;
  int line_start;
  int line_end;
  struct Location* next;
};
void Context_add_location(struct Context* context, struct Location* loc);
struct Location* Context_build_location(
    struct Context* context,
    struct AstNode* ast,
    int line_start,
    int line_end);
struct Location*
Context_get_location(struct Context* context, struct AstNode* ast);

#endif
