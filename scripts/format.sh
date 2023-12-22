#/usr/bin/env bash
find src include -name '*.c' -o -name '*.h' | xargs clang-format -i
find CMakeLists.txt src -name CMakeLists.txt | xargs cmake-format -i --command-case=lower --keyword-case=upper --enable-sort=true --autosort=true
