#include "ast/TypeTable.h"

#include "common/bsstring.h"
#include "common/ll-common.h"

#include <string.h>

// BUILTIN(name, size)
// ALIAS(name, alias_to)
// PTR_ALIAS(name, alias_to_basetpy)
#define BUILTIN_TYPES(BUILTIN, ALIAS, PTR_ALIAS)                               \
  BUILTIN(void, 0)                                                             \
  BUILTIN(int64, 8)                                                            \
  BUILTIN(int8, 1)                                                             \
  ALIAS(int, int64)                                                            \
  ALIAS(char, int8)                                                            \
  PTR_ALIAS(string, char)

static struct Type*
allocate_TypeWithName(struct Context* context, char* name, enum TypeKind kind) {
  struct Type* t = malloc(sizeof(*t));
  memset(t, 0, sizeof(*t));
  t->kind = kind;

  int len = strlen(name) + 1;
  t->name = malloc(sizeof(*t->name) * len);
  bsstrcpy(t->name, name);

  LL_APPEND(context->types, t);

  return t;
}

static struct TypeField* allocate_TypeField(char* name) {
  struct TypeField* t = malloc(sizeof(*t));
  memset(t, 0, sizeof(*t));

  int len = strlen(name) + 1;
  t->name = malloc(sizeof(*t->name) * len);
  bsstrcpy(t->name, name);
  return t;
}

static struct Type*
allocate_Builtin(struct Context* context, char* name, int size) {
  struct Type* t = allocate_TypeWithName(context, name, tk_BUILTIN);
  t->size = size;
  return t;
}

static struct Type*
allocate_Alias(struct Context* context, char* name, struct Type* alias_of) {
  struct Type* t = allocate_TypeWithName(context, name, tk_ALIAS);
  t->alias_of = alias_of;
  t->size = alias_of->size;
  return t;
}

static struct Type* allocate_Opaque(struct Context* context, char* name) {
  struct Type* t = allocate_TypeWithName(context, name, tk_OPAQUE);
  t->size = -1;
  return t;
}

// always allocates, want something that looks up the existing type
static struct Type*
allocate_Typedef(struct Context* context, char* name, struct AstNode* args) {
  struct Type* t = allocate_TypeWithName(context, name, tk_TYPEDEF);
  int size = 0;
  ast_foreach(args, a) {
    ASSERT(ast_verify_Variable(a));

    struct TypeField* newField =
        allocate_TypeField(ast_Identifier_name(ast_Variable_name(a)));
    newField->type = TypeTable_get_type_ast(context, ast_Variable_type(a));
    newField->parentType = t;
    size += newField->type->size;
    LL_APPEND(t->fields, newField);
  }
  t->size = size;
  return t;
}

char* make_ptr_name(char* name) {
  int len = strlen(name) + 2; // 1 for null, 1 for *
  char* ptr_name = malloc(sizeof(*ptr_name) * len);
  bsstrcpy(ptr_name, name);
  ptr_name[len - 2] = '*';
  ptr_name[len - 1] = '\0';
  return ptr_name;
}
static struct Type*
allocate_Pointer(struct Context* context, struct Type* pointer_to) {
  struct Type* t = allocate_TypeWithName(
      context,
      make_ptr_name(pointer_to->name),
      tk_POINTER);
  t->pointer_to = pointer_to;
  t->size = 8;
  return t;
}

void Context_install_builtin_types(struct Context* context) {

#define MAKE_BUILTIN(name, size)                                               \
  do {                                                                         \
    allocate_Builtin(context, #name, size);                                    \
  } while(0);
#define MAKE_ALIAS(name, alias_to)                                             \
  do {                                                                         \
    struct Type* t_alias_to = TypeTable_get_type(context, #alias_to);          \
    allocate_Alias(context, #name, t_alias_to);                                \
  } while(0);
#define MAKE_PTR_ALIAS(name, alias_to)                                         \
  do {                                                                         \
    struct Type* t_alias_to = TypeTable_get_ptr_type(                          \
        context,                                                               \
        TypeTable_get_type(context, #alias_to));                               \
    allocate_Alias(context, #name, t_alias_to);                                \
  } while(0);

  BUILTIN_TYPES(MAKE_BUILTIN, MAKE_ALIAS, MAKE_PTR_ALIAS)
#undef MAKE_BUILTIN
#undef MAKE_ALIAS
#undef MAKE_PTR_ALIAS
}

void TypeTable_add_from_ast(struct Context* context, struct AstNode* root) {
  ast_foreach(root, a) {
    TypeTable_get_type_ast(context, a);
    ast_foreach_child(a, c) { TypeTable_add_from_ast(context, c); }
  }
}
int TypeTable_equivalent_types(
    __attribute__((unused)) struct Context* context,
    struct Type* t1,
    struct Type* t2) {
  t1 = TypeTable_get_base_type(t1);
  t2 = TypeTable_get_base_type(t2);
  ASSERT(t1->kind != tk_ALIAS && t2->kind != tk_ALIAS);

  // if both typedef, check name
  if(t1->kind == tk_TYPEDEF && t2->kind == tk_TYPEDEF) {
    return strcmp(t1->name, t2->name) == 0;
  }

  // if both builtin, check ptr equivalence
  if(t1->kind == tk_BUILTIN && t2->kind == tk_BUILTIN) {
    return t1 == t2;
  }

  // if both ptr, check pointer_to equivalence
  if(t1->kind == tk_POINTER && t2->kind == tk_POINTER) {
    return TypeTable_get_base_type(t1->pointer_to) ==
           TypeTable_get_base_type(t2->pointer_to);
  }
  return 0;
}
struct Type* TypeTable_get_type(struct Context* context, char* name) {
  LL_FOREACH(context->types, type) {
    if(strcmp(type->name, name) == 0) return type;
  }
  return NULL;
}
struct Type*
TypeTable_get_type_ast(struct Context* context, struct AstNode* ast) {
  if(ast_is_type(ast, ast_Type)) {
    // if type exists by name, return it
    char* name = ast_Typename_name(ast_Type_name(ast));
    struct Type* t = TypeTable_get_type(context, name);
    if(t) return t;

    // build appropriate type
    if(ast_Type_is_opaque(ast)) {
      t = allocate_Opaque(context, name);
    } else {
      if(ast_Type_is_alias(ast)) {
        struct Type* alias_of =
            TypeTable_get_type_ast(context, ast_Type_alias(ast));
        if(!alias_of) return NULL;
        t = allocate_Alias(context, name, alias_of);
      } else {
        t = allocate_Typedef(context, name, ast_Type_args(ast));
      }
    }
    return t;

  } else if(ast_is_type(ast, ast_Typename)) {
    // get the type for typename, building ptr types as needed
    struct Type* base_type =
        TypeTable_get_type(context, ast_Typename_name(ast));
    if(!base_type) return NULL;
    struct Type* ptr_type = base_type;
    int ptr_level = ast_Typename_ptr_level(ast);
    while(ptr_level != 0) {
      ptr_type = TypeTable_get_ptr_type(context, ptr_type);
      ptr_level--;
    }
    return ptr_type;
  }
  return NULL;
}
struct Type* TypeTable_get_ptr_type(struct Context* context, struct Type* t) {
  char* ptr_name = make_ptr_name(t->name);
  LL_FOREACH(context->types, type) {
    if(strcmp(type->name, ptr_name) == 0) return type;
  }
  struct Type* ptr_type = allocate_Pointer(context, t);
  return ptr_type;
}
// follow alias chains to base type
struct Type* TypeTable_get_base_type(struct Type* t) {
  struct Type* ret = t;
  while(ret->kind == tk_ALIAS) {
    ret = ret->alias_of;
  }
  return ret;
}

int TypeTable_get_num_fields(struct Type* t) {
  ASSERT(Type_is_typedef(t));
  int n = 0;
  LL_FOREACH(t->fields, f) { n++; }
  return n;
}

int Type_is_pointer(struct Type* t) {
  return t->kind == tk_POINTER && t->pointer_to != NULL;
}
int Type_is_opaque(struct Type* t) { return t->kind == tk_OPAQUE; }

int Type_get_size(struct Type* t) { return TypeTable_get_base_type(t)->size; }

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
