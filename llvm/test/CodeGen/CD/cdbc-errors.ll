; RUN: not --crash llc -mtriple=cd-unknown-unknown -O0 %s -o - 2>&1 | FileCheck %s

define i32 @main() {
entry:
  %value = select i1 true, i32 1, i32 0
  ret i32 %value
}

; CHECK: CD target does not support LLVM instruction: select
