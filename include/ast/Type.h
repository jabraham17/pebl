#ifndef TYPE_H_
#define TYPE_H_
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

int Type_ptr_size();
struct Type* Type_int_type(struct Context* ctx, int size);

int Type_eq(struct Type* t1, struct Type* t2);

char* Type_to_string(struct Type* t);

struct Type* Type_get_ptr_type(struct Type* t);
// follow alias chains to base type
struct Type* Type_get_base_type(struct Type* t);

int Type_get_num_fields(struct Type* t);
int Type_is_pointer(struct Type* t);
int Type_is_opaque(struct Type* t);

int Type_is_signed(struct Type* t);
int Type_is_integer(struct Type* t);
int Type_is_void(struct Type* t);
int Type_is_boolean(struct Type* t);

int Type_get_size(struct Type* t);

int Type_is_typedef(struct Type* t);
struct TypeField* Type_get_TypeField(struct Type* t, char* name);
int TypeField_get_index(struct TypeField* tf);
// returns the offset in bits, assuming a PACKED struct
// this does not take alignment or padding into account
int TypeField_get_offset(struct TypeField* tf);
#endif
