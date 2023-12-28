#ifndef IR_CODEGEN_LLVM_H_
#define IR_CODEGEN_LLVM_H_

#include "context/context.h"

#include <llvm-c/Core.h>

struct cg_value {
  struct ScopeSymbol* variable;
  struct Type* type;
  LLVMValueRef value;
  LLVMTypeRef cg_type;
  struct cg_value* next;
};

struct cg_function {
  char* name;
  char* mname; // mangled
  struct Type* rettype;
  LLVMValueRef function;
  LLVMTypeRef cg_type;
  int is_external;
  struct cg_function* next;
};
struct cg_context {
  LLVMContextRef llvmContext;
  LLVMModuleRef module;
  LLVMBuilderRef builder;
  char* outfilename;
  struct cg_value* current_values;
  struct cg_function* functions;
  struct cg_type* struct_types;
};

void init_cg_context(struct Context* context, char* filename);
void deinit_cg_context(struct Context* context);
void codegen(struct Context* context);
void cg_emit(struct Context* context);

#endif
