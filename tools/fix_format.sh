find ../include/ -iname *.h | xargs clang-format-16 -i
find ../src/ -iname *.cpp | xargs clang-format-16 -i
