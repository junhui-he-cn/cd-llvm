; RUN: not --crash llc -mtriple=cd-unknown-unknown -O0 %s -o - 2>&1 | FileCheck %s

define i32 @main() {
entry:
  %value = udiv i32 7, 2
  ret i32 %value
}

; CHECK: CD target does not support LLVM instruction: udiv
