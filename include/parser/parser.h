#ifndef PEBL_PARSER_H_
#define PEBL_PARSER_H_

#include "lexer.h"

#include "ast/ast.h"
#include "context/context.h"

void parser_init(struct Context* context);
void parser_parse(struct Context* context);

#endif
