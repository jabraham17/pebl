# c interop

Currently the compiler supports making functions exposed to the outside world by using the `export` keyword. It would be nice to have a script (compiler flag?) which generates a c header file for all exports. This would really help c interop.

The following pebl code would become the following header
```pebl
# in myLib.pebl
export func foo(a:int*):int {
  ...
}
```

```c++
// in myLib.h
#ifndef PEBL_MYLIB_H_
#define PEBL_MYLIB_H_

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

int64_t foo(int64_t* a);

#ifdef __cplusplus
}
#endif

#endif
```

This header could be used with either C or C++.
