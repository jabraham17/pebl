#ifndef DEBUGWRAPPERS_H_
#define DEBUGWRAPPERS_H_
#include <llvm-c/DebugInfo.h>

LLVMMetadataRef PeblDICreateCompileUnit(
    LLVMDIBuilderRef Builder,
    LLVMMetadataRef FileRef,
    LLVMBool isOptimized,
    const char* Flags);

LLVMMetadataRef PeblDICreateFile(
    LLVMDIBuilderRef Builder,
    const char* Filename,
    const char* Directory);
#endif
