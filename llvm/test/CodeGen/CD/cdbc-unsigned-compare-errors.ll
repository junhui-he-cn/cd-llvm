; RUN: not --crash llc -mtriple=cd-unknown-unknown -O0 %s -o - 2>&1 | FileCheck %s

define i32 @main() {
entry:
  %value = icmp ult i32 -1, 1
  ret i32 0
}

; CHECK: CD target does not support LLVM operation: unsigned integer comparison predicate
