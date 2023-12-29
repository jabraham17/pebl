#include "ast/Type.h"

#include "ast/scope-resolve.h"
#include "common/bsstring.h"
#include "common/ll-common.h"
#include "context/context.h"

#include <string.h>

// TODO: DUPLICATED in scope resolve
static struct Type* Type_allocate(char* name) {
  struct Type* t = malloc(sizeof(*t));
  memset(t, 0, sizeof(*t));

  int len = strlen(name) + 1;
  t->name = malloc(sizeof(*t->name) * len);
  bsstrcpy(t->name, name);

  return t;
}

int Type_ptr_size() { return 64; }
struct Type* Type_int_type(struct Context* ctx, int size) {
  char* name;
  if(size == 8) name = "int8";
  else if(size == 64) name = "int64";
  else UNIMPLEMENTED("unknown number size '%d'\n", size);

  struct ScopeResult* scope = ctx->scope_table;
  return scope_get_Type_from_name(ctx, scope, name, 1);
}

static struct Type* Type_allocate_Pointer(struct Type* pointer_to) {
  struct Type* t = Type_allocate(pointer_to->name);
  t->kind = tk_POINTER;
  t->pointer_to = pointer_to;
  t->size = Type_ptr_size();
  return t;
}

int Type_eq(struct Type* t1, struct Type* t2) {
  // follow alias chains
  t1 = Type_get_base_type(t1);
  t2 = Type_get_base_type(t2);
  ASSERT(t1->kind != tk_ALIAS && t2->kind != tk_ALIAS);

  // if both ptr, check pointer_to equivalence
  if(t1->kind == tk_POINTER && t2->kind == tk_POINTER) {
    return Type_eq(
        Type_get_base_type(t1->pointer_to),
        Type_get_base_type(t2->pointer_to));
  }

  // check name
  return t1->kind == t2->kind && strcmp(t1->name, t2->name) == 0;
}

char* Type_to_string(struct Type* type) {
  char* name = type->name;
  struct Type* t = type;
  int num_stars = 0;
  while(Type_is_pointer(t)) {
    num_stars += 1;
    t = t->pointer_to;
  }
  if(num_stars > 0) {
    char* stars = malloc(sizeof(*stars) * (num_stars + 1));
    for(int i = 0; i < num_stars; i++)
      stars[i] = '*';
    stars[num_stars] = '\0';
    name = bsstrcat(name, stars);
  }
  return name;
}

struct Type* Type_get_ptr_type(struct Type* t) {
  struct Type* ptr_type = Type_allocate_Pointer(t);
  return ptr_type;
}
// follow alias chains to base type
struct Type* Type_get_base_type(struct Type* t) {
  struct Type* ret = t;
  while(ret->kind == tk_ALIAS) {
    ret = ret->alias_of;
  }
  return ret;
}

int Type_get_num_fields(struct Type* t) {
  ASSERT(Type_is_typedef(t));
  int n = 0;
  LL_FOREACH(t->fields, f) { n++; }
  return n;
}

int Type_is_pointer(struct Type* t) {
  struct Type* base_type = Type_get_base_type(t);
  return base_type->kind == tk_POINTER && base_type->pointer_to != NULL;
}
int Type_is_opaque(struct Type* t) {
  struct Type* base_type = Type_get_base_type(t);
  return base_type->kind == tk_OPAQUE;
}

int Type_is_signed(struct Type* t) {
  // all integers are signed, everything else is unsigned
  return Type_is_integer(t);
}
int Type_is_integer(struct Type* t) {
  struct Type* base_type = Type_get_base_type(t);
  if(base_type->kind == tk_BUILTIN) {
    return strcmp(base_type->name, "int64") == 0 ||
           strcmp(base_type->name, "int8") == 0;
  }
  return 0;
}
int Type_is_void(struct Type* t) {
  struct Type* base_type = Type_get_base_type(t);
  return base_type->kind == tk_BUILTIN && strcmp(base_type->name, "void") == 0;
}
int Type_is_boolean(struct Type* t) {
  struct Type* base_type = Type_get_base_type(t);
  return base_type->kind == tk_BUILTIN && strcmp(base_type->name, "bool") == 0;
}

int Type_get_size(struct Type* t) { return Type_get_base_type(t)->size; }

int Type_is_typedef(struct Type* t) { return t->kind == tk_TYPEDEF; }
struct TypeField* Type_get_TypeField(struct Type* t, char* name) {
  ASSERT(Type_is_typedef(t));
  LL_FOREACH(t->fields, field) {
    if(strcmp(field->name, name) == 0) {
      return field;
    }
  }
  return NULL;
}
int TypeField_get_index(struct TypeField* tf) {
  int idx = 0;
  LL_FOREACH(tf->parentType->fields, field) {
    if(tf == field) return idx;
    idx++;
  }
  return -1;
}
int TypeField_get_offset(struct TypeField* tf) {
  int offset = 0;
  LL_FOREACH(tf->parentType->fields, field) {
    if(tf == field) return offset;
    offset += Type_get_size(field->type);
  }
  return -1;
}
