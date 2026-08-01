; RUN: not --crash llc -mtriple=cd-unknown-unknown -O0 %s -o - 2>&1 | FileCheck %s

define { i32 } @main() {
entry:
  ret { i32 } zeroinitializer
}

; CHECK: CD target does not support LLVM operation: non-scalar function return values
