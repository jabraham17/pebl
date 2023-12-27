#ifndef TYPE_TABLE_H_
#define TYPE_TABLE_H_
#include "ast.h"

#include "context/context.h"

enum TypeKind {
  tk_BUILTIN,
  tk_ALIAS,
  tk_TYPEDEF,
  tk_OPAQUE,
  tk_POINTER,
};

struct TypeField;

struct Type {
  enum TypeKind kind;
  char* name;
  struct Type* next;
  int size;

  // optional
  struct Type* alias_of;    // valid for tk_ALIAS
  struct TypeField* fields; // valid for tk_TYPEDEF
  struct Type* pointer_to;  // valid for tk_POINTER
};

struct TypeField {
  char* name;
  struct Type* type;
  struct Type* parentType;
  struct TypeField* next;
};

void Context_install_builtin_types(struct Context* context);

void TypeTable_add_from_ast(struct Context* context, struct AstNode* ast);

int TypeTable_equivalent_types(
    struct Context* context,
    struct Type* t1,
    struct Type* t2);
struct Type* TypeTable_get_type(struct Context* context, char* name);
struct Type*
TypeTable_get_type_ast(struct Context* context, struct AstNode* ast);
struct Type* TypeTable_get_ptr_type(struct Context* context, struct Type* t);
// follow alias chains to base type
struct Type* TypeTable_get_base_type(struct Type* t);

int TypeTable_get_num_fields(struct Type* t);
int Type_is_pointer(struct Type* t);
int Type_is_opaque(struct Type* t);

int Type_get_size(struct Type* t);

int Type_is_typedef(struct Type* t);
struct TypeField* Type_get_TypeField(struct Type* t, char* name);
int TypeField_get_index(struct TypeField* tf);
int TypeField_get_offset(struct TypeField* tf);
#endif
