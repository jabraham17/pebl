#include "debugwrappers.h"

#include <stdlib.h>
#include <string.h>

LLVMMetadataRef PeblDICreateCompileUnit(
    LLVMDIBuilderRef Builder,
    LLVMMetadataRef FileRef,
    LLVMBool isOptimized,
    const char* Flags) {

  char* Producer = "pebl compiler";
  int ProducerLen = strlen(Producer);
  int FlagsLen = Flags ? strlen(Flags) : 0;
  return LLVMDIBuilderCreateCompileUnit(
      Builder,
      LLVMDWARFSourceLanguageC,
      FileRef,
      Producer,
      ProducerLen,
      isOptimized,
      Flags,
      FlagsLen,
      0,
      NULL,
      0,
      /*Kind*/ LLVMDWARFEmissionFull,
      0,
      0,
      0,
      NULL,
      0,
      NULL,
      0);
}

LLVMMetadataRef PeblDICreateFile(
    LLVMDIBuilderRef Builder,
    const char* Filename,
    const char* Directory) {

  int FilenameLen = strlen(Filename);
  int DirectoryLen = strlen(Directory);

  return LLVMDIBuilderCreateFile(
      Builder,
      Filename,
      FilenameLen,
      Directory,
      DirectoryLen);
}
