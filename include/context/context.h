
#ifndef CONTEXT_H_
#define CONTEXT_H_

#include "Arguments.h"

#include "ast/location.h"

#include <stdio.h>
#include <wchar.h>

struct lexer_state;
struct AstNode;
struct Location;
struct Type;
struct ScopeResult;
struct cg_context;
struct CompilerBuiltin;

struct Arguments;

struct Context {
  struct Arguments* arguments;
  struct lexer_state* lexer;

  struct AstNode* ast;
  struct Location* locations;

  struct ScopeResult* scope_table;

  struct CompilerBuiltin* compiler_builtins;

  struct cg_context* codegen;
};

struct Context* Context_allocate();
void Context_init(struct Context* context, struct Arguments* arguments);

void BREAKPOINT();

#define FATAL_EXIT()                                                           \
  do {                                                                         \
    BREAKPOINT();                                                              \
    exit(1);                                                                   \
  } while(0)

#define WARNING_ON_AST(context, ast, format, ...)                              \
  do {                                                                         \
    struct Location* loc = Context_get_location(context, ast);                 \
    if(loc) {                                                                  \
      fwprintf(                                                                \
          stderr,                                                              \
          L"%s:%d: warning: " format,                                          \
          Arguments_inFilename(context->arguments),                            \
          loc->line_start,                                                     \
          ##__VA_ARGS__);                                                      \
    } else {                                                                   \
      fwprintf(                                                                \
          stderr,                                                              \
          L"%s: warning: " format,                                             \
          Arguments_inFilename(context->arguments),                            \
          ##__VA_ARGS__);                                                      \
    }                                                                          \
    BREAKPOINT();                                                              \
  } while(0)

#define WARNING(context, format, ...)                                          \
  do {                                                                         \
    WARNING_ON_AST(context, NULL, format, ##__VA_ARGS__);                      \
  } while(0)

#define ASSERT_MSG(cond, format, ...)                                          \
  do {                                                                         \
    if(!(cond)) {                                                              \
      fwprintf(stderr, L"Assert failed: " format, ##__VA_ARGS__);              \
      FATAL_EXIT();                                                            \
    }                                                                          \
  } while(0)
#define ASSERT(cond) ASSERT_MSG(cond, #cond "\n")

#define UNIMPLEMENTED(format, ...)                                             \
  do {                                                                         \
    fwprintf(stderr, L"Unimplemented: " format, ##__VA_ARGS__);                \
    FATAL_EXIT();                                                              \
  } while(0)

#define ERROR_ON_LINE(context, lineno, format, ...)                            \
  do {                                                                         \
    fwprintf(                                                                  \
        stderr,                                                                \
        L"%s:%d: error: " format,                                              \
        Arguments_inFilename(context->arguments),                              \
        lineno,                                                                \
        ##__VA_ARGS__);                                                        \
    FATAL_EXIT();                                                              \
  } while(0)

#define ERROR_ON_AST(context, ast, format, ...)                                \
  do {                                                                         \
    struct Location* loc = Context_get_location(context, ast);                 \
    if(loc) {                                                                  \
      ERROR_ON_LINE(context, loc->line_start, format, ##__VA_ARGS__);          \
    } else {                                                                   \
      fwprintf(                                                                \
          stderr,                                                              \
          L"%s: error: " format,                                               \
          Arguments_inFilename(context->arguments),                            \
          ##__VA_ARGS__);                                                      \
    }                                                                          \
    FATAL_EXIT();                                                              \
  } while(0)

#define ERROR(context, format, ...)                                            \
  do {                                                                         \
    ERROR_ON_AST(context, NULL, format, ##__VA_ARGS__);                        \
  } while(0)

#endif
