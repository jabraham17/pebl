#/usr/bin/env bash
find src include -name '*.c' -o -name '*.h' | xargs \
  clang-format -i

find CMakeLists.txt src cmake -name CMakeLists.txt -o -name '*.cmake*' | xargs \
  cmake-format -i --command-case=lower --keyword-case=upper --enable-sort=true --autosort=true

find src -name '*.py' | xargs \
  python3 -m black
