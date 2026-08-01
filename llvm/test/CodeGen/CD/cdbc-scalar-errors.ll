; RUN: not --crash llc -mtriple=cd-unknown-unknown -O0 %s -o - 2>&1 | FileCheck %s

define i64 @main() {
entry:
  ret i64 9007199254740993
}

; CHECK: CD target integer constant is not exactly representable as a number
